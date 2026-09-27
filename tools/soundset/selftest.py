#!/usr/bin/env python3
"""Self-test for the Sound Set builder: builds a fake library laid out exactly like
Karoryfer Emilyguitar (Windows paths, lorand/hirand RR, noises, release trigger) and
checks the imported result."""
import json, os, shutil, subprocess, sys, tempfile, wave
from collections import Counter
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SR = 44100

def w24(path, x):
    x = np.clip(x, -1, 1); i = (x * 8388607).astype(np.int32)
    b = np.stack([i & 255, (i >> 8) & 255, (i >> 16) & 255], axis=1).astype(np.uint8).tobytes()
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(3); w.setframerate(SR); w.writeframes(b)

def make_library(root):
    for d in ("notes", "noises", "release"):
        os.makedirs(os.path.join(root, d), exist_ok=True)
    sfz, detune = [], {"rr1": 7.0, "rr2": -4.0, "rr3": 0.0}
    for kc, nm in {40: "e2", 45: "a2", 48: "c3"}.items():
        for li, (lay, lo, hi) in enumerate([("p", 1, 40), ("mp", 41, 80), ("mf", 81, 120), ("f", 121, 127)]):
            sfz.append("<group>\nlokey=%d\nhikey=%d\nlovel=%d\nhivel=%d\npitch_keycenter=%d" % (kc - 1, kc + 1, lo, hi, kc))
            for k, (rr, c) in enumerate(detune.items()):
                f = 440 * 2 ** ((kc - 69) / 12) * 2 ** (c / 1200)
                t = np.arange(int(SR * 1.2)) / SR; x = np.zeros_like(t); on = int(0.02 * SR); tt = t[on:] - t[on]
                x[on:] = (0.2 + 0.2 * li) * np.exp(-tt * 2) * (np.sin(2 * np.pi * f * tt) + 0.4 * np.sin(4 * np.pi * f * tt))
                w24(os.path.join(root, "notes", "%s_%s_%s.wav" % (nm, lay, rr)), x)
                rng = ["hirand=0.333", "lorand=0.333\nhirand=0.666", "lorand=0.666"][k]
                sfz.append("<region>\nsample=notes\\%s_%s_%s.wav\n%s" % (nm, lay, rr, rng))
    sfz.append("//====\n<group>\nlokey=90\nhikey=90\npitch_keycenter=90")
    for k in range(5):
        w24(os.path.join(root, "noises", "Fingering1_rr%d.wav" % (k + 1)), np.random.randn(8000) * 0.05)
        sfz.append("<region>\nsample=noises\\fingering1_rr%d.wav\nlorand=%.1f\nhirand=%.1f" % (k + 1, k * 0.2, (k + 1) * 0.2))
    sfz.append("<group> trigger=release lokey=33 hikey=89 pitch_keycenter=60 // release noises")
    for k in range(4):
        w24(os.path.join(root, "release", "release_rr%d.wav" % (k + 1)), np.random.randn(6000) * 0.03)
        sfz.append("<region> sample=release\\release_rr%d.wav lorand=%.2f hirand=%.2f" % (k + 1, k * 0.25, (k + 1) * 0.25))
    open(os.path.join(root, "emily_basic.sfz"), "w").write("\n".join(sfz))

def main():
    tmp = tempfile.mkdtemp()
    try:
        lib = os.path.join(tmp, "lib"); make_library(lib)
        cfg = json.load(open(os.path.join(HERE, "sources.json")))
        cfg["guitars"] = [g for g in cfg["guitars"] if g["id"] == "sg"]
        src = os.path.join(tmp, "sources.json"); json.dump(cfg, open(src, "w"))
        out = os.path.join(tmp, "SoundSet")
        subprocess.check_call([sys.executable, os.path.join(HERE, "build_soundset.py"), "--sources", src,
                               "--local", "sg=" + lib, "--out", out, "--work", os.path.join(tmp, "w"),
                               "--zip", os.path.join(tmp, "set.zip")])
        g = json.load(open(os.path.join(out, "sg", "guitar.json")))
        z = g["zones"]
        kinds = Counter(x["kind"] for x in z)
        rr = Counter(Counter((x["kind"], x["slot"]) for x in z if x["kind"] == "note").values())
        tunes = sorted(set(round(x["tune"]) for x in z if x["kind"] == "note"))
        fails = []
        if kinds != Counter({"note": 36, "noise": 5, "release": 4}): fails.append("zone kinds %s" % kinds)
        if rr != Counter({3: 12}): fails.append("round robins per slot %s" % rr)
        if tunes != [-7, 0, 4]: fails.append("tuning fixes %s (expected -7, 0, 4)" % tunes)
        onset = g["samples"][0]["onset"]
        if not (830 <= onset <= 882): fails.append("onset %d" % onset)
        for s in g["samples"]:
            if not os.path.exists(os.path.join(out, "sg", s["file"])): fails.append("missing " + s["file"])
        idx = json.load(open(os.path.join(out, "soundset.json")))
        if [x["id"] for x in idx["guitars"]] != ["sg"]: fails.append("index %s" % idx)
        print("zones %s | RR %s | tuning fixes %s cents | onset %d" % (dict(kinds), dict(rr), tunes, onset))
        if fails:
            print("FAILED:\n  " + "\n  ".join(fails)); return 1
        print("SOUND SET BUILDER OK")
        return 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

if __name__ == "__main__":
    sys.exit(main())
