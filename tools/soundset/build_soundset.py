#!/usr/bin/env python3
"""
DaliGTR Sound Set builder.

Clones open-source SFZ guitar libraries, imports their mapping, analyses every recording
(real onset, tuning, level) and writes the DaliGTR format:

    <out>/soundset.json              index of guitars
    <out>/<id>/guitar.json           zones + samples (DGL v1)
    <out>/<id>/samples/*.flac|wav    audio (FLAC when python-soundfile is installed)

Usage:
    python build_soundset.py --sources sources.json --out build/SoundSet --zip DaliGTR-SoundSet.zip
    python build_soundset.py --sources sources.json --inspect report.md        (mapping report for all sources)
    python build_soundset.py --sources sources.json --local sg=/path/to/lib    (use a local folder, no git)
"""
import argparse, json, os, re, shutil, subprocess, sys, wave, zipfile
import numpy as np

try:
    import soundfile as sf
except ImportError:
    sf = None

# ----------------------------------------------------------------------------- SFZ parsing
NOTE_NAMES = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}

def parse_key(v):
    v = str(v).strip().lower()
    if re.fullmatch(r"-?\d+", v):
        return int(v)
    m = re.fullmatch(r"([a-g])([#b]?)(-?\d+)", v)
    if not m:
        raise ValueError("bad key: " + v)
    n = NOTE_NAMES[m.group(1)] + (1 if m.group(2) == "#" else -1 if m.group(2) == "b" else 0)
    return n + (int(m.group(3)) + 1) * 12      # SFZ: c4 = 60

def read_sfz_text(path, defines=None, depth=0):
    if depth > 8:
        raise RuntimeError("#include nesting too deep")
    defines = {} if defines is None else defines
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    out = []
    for line in text.splitlines():
        s = line.strip()
        m = re.match(r'#define\s+(\$\w+)\s+(.*)', s)
        if m:
            defines[m.group(1)] = m.group(2).strip()
            continue
        m = re.match(r'#include\s+"([^"]+)"', s)
        if m:
            inc = resolve_path(os.path.dirname(path), m.group(1))
            out.append(read_sfz_text(inc, defines, depth + 1))
            continue
        for k in sorted(defines, key=len, reverse=True):
            line = line.replace(k, defines[k])
        out.append(line)
    return "\n".join(out)

TOKEN = re.compile(r"<(\w+)>|([A-Za-z0-9_]+)=")

def parse_sfz(path):
    """Returns (regions, control). Each region is a dict of opcodes with inheritance applied."""
    text = read_sfz_text(path)
    matches = list(TOKEN.finditer(text))
    scopes = {"global": {}, "master": {}, "group": {}}
    control, regions, current, header = {}, [], None, None

    def flush():
        if current is not None:
            regions.append(current)

    for i, m in enumerate(matches):
        end = matches[i + 1].start() if i + 1 < len(matches) else len(text)
        if m.group(1):
            flush()
            header = m.group(1).lower()
            current = None
            if header == "global":
                scopes = {"global": {}, "master": {}, "group": {}}
            elif header == "master":
                scopes["master"], scopes["group"] = {}, {}
            elif header == "group":
                scopes["group"] = {}
            elif header == "region":
                current = {**scopes["global"], **scopes["master"], **scopes["group"]}
            continue
        key = m.group(2).lower()
        raw = text[m.end():end]
        value = raw.strip() if key == "sample" else (raw.split()[0] if raw.split() else "")
        if key == "sample":
            value = value.splitlines()[0].strip() if value else value
        if header == "control":
            control[key] = value
        elif header in ("global", "master", "group"):
            scopes[header][key] = value
        elif header == "region" and current is not None:
            current[key] = value
    flush()
    return regions, control

def resolve_path(base, rel):
    """Case-insensitive path resolution; SFZ files often use Windows backslashes."""
    parts = [p for p in rel.replace("\\", "/").split("/") if p not in ("", ".")]
    cur = base
    for p in parts:
        if p == "..":
            cur = os.path.dirname(cur)
            continue
        cand = os.path.join(cur, p)
        if not os.path.exists(cand) and os.path.isdir(cur):
            low = {e.lower(): e for e in os.listdir(cur)}
            if p.lower() in low:
                cand = os.path.join(cur, low[p.lower()])
        cur = cand
    return cur

# ----------------------------------------------------------------------------- audio
def read_audio(path):
    """-> (float32 array [frames, channels], samplerate)"""
    if sf is not None:
        data, sr = sf.read(path, dtype="float32", always_2d=True)
        return data, sr
    with wave.open(path, "rb") as w:
        ch, width, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 2:
        a = np.frombuffer(raw, dtype="<i2").astype(np.float32) / 32768.0
    elif width == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        i = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16))
        i = np.where(i & 0x800000, i - 0x1000000, i)
        a = i.astype(np.float32) / 8388608.0
    elif width == 4:
        a = np.frombuffer(raw, dtype="<i4").astype(np.float32) / 2147483648.0
    else:
        raise RuntimeError("unsupported WAV width %d in %s" % (width, path))
    return a.reshape(-1, ch), sr

def find_onset(mono, sr):
    peak = float(np.max(np.abs(mono))) if len(mono) else 0.0
    if peak <= 0:
        return 0, 0.0
    idx = int(np.argmax(np.abs(mono) > peak * 10 ** (-36 / 20)))
    return max(0, idx - int(0.001 * sr)), peak

def measure_cents(mono, sr, onset, expected_hz):
    """Tuning deviation of a recording vs its key centre, via normalised autocorrelation."""
    start = onset + int(0.06 * sr)
    n = int(0.25 * sr)
    x = mono[start:start + n + int(sr / 50)]
    if len(x) < n + int(sr / 50):
        return 0.0, 0.0
    x = x - np.mean(x)
    lo, hi = max(2, int(sr / (expected_hz * 1.25))), int(sr / (expected_hz * 0.8)) + 1
    def corr(lag):
        a, b = x[:n], x[lag:lag + n]
        d = np.sqrt(np.dot(a, a) * np.dot(b, b)) + 1e-30
        return float(np.dot(a, b) / d)
    cv = {lag: corr(lag) for lag in range(lo - 1, hi + 2)}
    best = max(range(lo, hi + 1), key=lambda l: cv[l])
    conf = cv[best]
    a, b, c = cv[best - 1], cv[best], cv[best + 1]
    den = a - 2 * b + c
    period = best + (0.5 * (a - c) / den if den != 0 else 0.0)
    measured = sr / period
    return 1200.0 * np.log2(expected_hz / measured), conf

def write_audio(src_path, data, sr, dst_noext):
    if sf is not None:
        dst = dst_noext + ".flac"
        sf.write(dst, data, sr, format="FLAC", subtype="PCM_24")
    else:
        dst = dst_noext + ".wav"
        shutil.copyfile(src_path, dst)
    return dst

# ----------------------------------------------------------------------------- import
def classify(region, src):
    sample = region.get("sample", "").replace("\\", "/")
    if src.get("skip") and re.search(src["skip"], sample, re.I):
        return None
    if region.get("trigger", "attack").lower() in ("release", "release_key"):
        return "release"
    if src.get("noise") and re.search(src["noise"], sample, re.I):
        return "noise"
    return "note"

def noise_type(sample):
    base = os.path.splitext(os.path.basename(sample.replace("\\", "/")))[0].lower()
    base = re.sub(r"_?rr\d+$", "", base)
    return re.sub(r"\d+$", "", base) or base

def import_guitar(src, lib_dir, out_dir, log):
    sfz_path = resolve_path(lib_dir, src["sfz"])
    regions, control = parse_sfz(sfz_path)
    default_path = control.get("default_path", "")
    base = os.path.dirname(sfz_path)
    gdir = os.path.join(out_dir, src["id"])
    os.makedirs(os.path.join(gdir, "samples"), exist_ok=True)

    samples, zones, sample_index, slots = [], [], {}, {}
    tune_fixes, skipped = [], 0
    for r in regions:
        kind = classify(r, src)
        if kind is None or "sample" not in r:
            skipped += 1
            continue
        if src.get("keyswitch") is not None and "sw_last" in r and parse_key(r["sw_last"]) != src["keyswitch"]:
            skipped += 1
            continue
        rel = (default_path + r["sample"]).replace("\\", "/")
        path = resolve_path(base, rel)
        if not os.path.exists(path):
            log("  missing sample: %s" % rel)
            skipped += 1
            continue

        lo = parse_key(r.get("lokey", r.get("key", 0)))
        hi = parse_key(r.get("hikey", r.get("key", 127)))
        kc = parse_key(r.get("pitch_keycenter", r.get("key", (lo + hi) // 2)))
        lov, hiv = int(r.get("lovel", 1)), int(r.get("hivel", 127))
        tune = float(r.get("tune", 0)) + 100.0 * float(r.get("transpose", 0))

        if path not in sample_index:
            data, sr = read_audio(path)
            mono = data.mean(axis=1)
            onset, peak = find_onset(mono, sr)
            fix = 0.0
            if kind == "note":
                expected = 440.0 * 2 ** ((kc - 69 - tune / 100.0) / 12.0)
                cents, conf = measure_cents(mono, sr, onset, expected)
                if conf > 0.8 and abs(cents) < 60:
                    fix = cents
                    tune_fixes.append(cents)
            name = re.sub(r"[^A-Za-z0-9_.-]", "_", rel.replace("/", "_"))
            name = os.path.splitext(name)[0]
            dst = write_audio(path, data, sr, os.path.join(gdir, "samples", name))
            sample_index[path] = (len(samples), fix)
            samples.append({"file": "samples/" + os.path.basename(dst), "sr": sr, "channels": int(data.shape[1]),
                            "frames": int(data.shape[0]), "onset": int(onset), "peak": round(peak, 5)})
        si, fix = sample_index[path]

        slot_key = (kind, lo, hi, kc, lov, hiv, r.get("sw_last", ""), noise_type(r["sample"]) if kind == "noise" else "")
        slot = slots.setdefault(slot_key, len(slots))
        z = {"kind": kind, "sample": si, "loKey": lo, "hiKey": hi, "key": kc, "loVel": lov, "hiVel": hiv,
             "slot": slot, "tune": round(tune + fix, 2), "gain": float(r.get("volume", 0))}
        if kind == "noise":
            z["noise"] = noise_type(r["sample"])
        zones.append(z)

    guitar = {"format": "DGL", "version": 1, "id": src["id"], "name": src["name"], "credit": src.get("credit", ""),
              "releaseSec": src.get("release_sec", 0.1), "samples": samples, "zones": zones}
    with open(os.path.join(gdir, "guitar.json"), "w") as f:
        json.dump(guitar, f, indent=1)

    kinds = {}
    for z in zones:
        kinds[z["kind"]] = kinds.get(z["kind"], 0) + 1
    notes = [z for z in zones if z["kind"] == "note"]
    log("  %s: %d samples, zones %s, keys %s-%s, skipped %d, tuning fixes: %d (max %.1f cents)" % (
        src["id"], len(samples), kinds, min((z["loKey"] for z in notes), default="-"),
        max((z["hiKey"] for z in notes), default="-"), skipped, len(tune_fixes),
        max((abs(c) for c in tune_fixes), default=0.0)))
    return {"id": src["id"], "name": src["name"], "folder": src["id"], "credit": src.get("credit", "")}

# ----------------------------------------------------------------------------- inspect
def inspect_library(src, lib_dir):
    lines = ["## %s (%s)" % (src["name"], src["id"]), ""]
    sfzs = []
    for root, _, files in os.walk(lib_dir):
        for fn in files:
            if fn.lower().endswith(".sfz"):
                sfzs.append(os.path.relpath(os.path.join(root, fn), lib_dir))
    lines.append("SFZ files: " + ", ".join(sorted(sfzs)))
    for rel in sorted(sfzs):
        try:
            regions, control = parse_sfz(os.path.join(lib_dir, rel))
        except Exception as e:
            lines.append("- `%s`: parse error %s" % (rel, e))
            continue
        opcodes, triggers, folders, sw = {}, {}, {}, set()
        for r in regions:
            for k in r:
                opcodes[k] = opcodes.get(k, 0) + 1
            t = r.get("trigger", "attack"); triggers[t] = triggers.get(t, 0) + 1
            fdr = os.path.dirname(r.get("sample", "").replace("\\", "/")); folders[fdr] = folders.get(fdr, 0) + 1
            if "sw_last" in r: sw.add(r["sw_last"])
        lines.append("- `%s`: %d regions, control %s" % (rel, len(regions), control))
        lines.append("  - triggers: %s" % triggers)
        lines.append("  - sample folders: %s" % folders)
        lines.append("  - keyswitches (sw_last): %s" % sorted(sw))
        lines.append("  - opcodes: %s" % dict(sorted(opcodes.items(), key=lambda kv: -kv[1])))
    lines.append("")
    return "\n".join(lines)

# ----------------------------------------------------------------------------- main
def fetch(src, work, local):
    if src["id"] in local:
        return local[src["id"]]
    dst = os.path.join(work, src["id"])
    if not os.path.isdir(dst):
        subprocess.check_call(["git", "clone", "--depth", "1", src["repo"], dst])
    return dst

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sources", required=True)
    ap.add_argument("--work", default="build/soundset-src")
    ap.add_argument("--out", default="build/SoundSet")
    ap.add_argument("--zip")
    ap.add_argument("--inspect")
    ap.add_argument("--local", action="append", default=[], help="id=path")
    args = ap.parse_args()

    cfg = json.load(open(args.sources))
    local = dict(kv.split("=", 1) for kv in args.local)
    os.makedirs(args.work, exist_ok=True)
    log = lambda s: print(s, flush=True)

    if args.inspect:
        parts = ["# DaliGTR source inspection", ""]
        for src in cfg["guitars"]:
            try:
                parts.append(inspect_library(src, fetch(src, args.work, local)))
            except Exception as e:
                parts.append("## %s\nerror: %s\n" % (src["id"], e))
        open(args.inspect, "w").write("\n".join(parts))
        log("inspection report -> " + args.inspect)
        return 0

    os.makedirs(args.out, exist_ok=True)
    index = {"format": "DaliGTR-SoundSet", "version": cfg.get("soundset_version", 1), "guitars": []}
    for src in cfg["guitars"]:
        if not src.get("enabled", False):
            log("skip (disabled): " + src["id"])
            continue
        log("importing " + src["id"])
        index["guitars"].append(import_guitar(src, fetch(src, args.work, local), args.out, log))
    with open(os.path.join(args.out, "soundset.json"), "w") as f:
        json.dump(index, f, indent=1)

    if args.zip:
        with zipfile.ZipFile(args.zip, "w", zipfile.ZIP_STORED) as z:   # audio is already compressed
            for root, _, files in os.walk(args.out):
                for fn in files:
                    p = os.path.join(root, fn)
                    z.write(p, os.path.relpath(p, args.out))
        log("zip -> %s (%.1f MB)" % (args.zip, os.path.getsize(args.zip) / 1e6))
    return 0

if __name__ == "__main__":
    sys.exit(main())
