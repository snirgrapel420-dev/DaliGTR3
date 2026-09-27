#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "GuitarLoader.h"
#include <atomic>

#ifndef DALIGTR_SOUNDSET_URL
 #define DALIGTR_SOUNDSET_URL ""
#endif

namespace dgtr
{
// Owns the Sound Set on disk: first-run download from the GitHub release, install from a
// local zip, and background loading of the selected guitar. Audio thread only touches
// fetchIfChanged().
class SoundSetManager : private juce::Thread, private juce::Timer
{
public:
    enum class State { Missing, Downloading, Installing, Loading, Ready, Error };

    SoundSetManager() : juce::Thread ("DaliGTR SoundSet")
    {
        startThread (juce::Thread::Priority::background);
        startTimerHz (4);
    }

    ~SoundSetManager() override
    {
        stopTimer();
        signalThreadShouldExit();
        notify();
        stopThread (8000);
    }

    static juce::File soundSetFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Dali Audio").getChildFile ("DaliGTR").getChildFile ("Sounds");
    }

    bool isInstalled() const { return soundSetFolder().getChildFile ("soundset.json").existsAsFile(); }
    static juce::String downloadUrl() { return DALIGTR_SOUNDSET_URL; }

    // ---- message thread API ---------------------------------------------------------
    void requestDownload()                        { post (Job::Download, {}); }
    void requestInstallZip (const juce::File& z)  { post (Job::InstallZip, z); }

    // desired guitar id; loaded in the background when it changes
    void setDesiredGuitar (const juce::String& id)
    {
        const juce::ScopedLock sl (lock);
        if (id == desiredGuitar) return;
        desiredGuitar = id;
        guitarDirty = true;
        notify();
    }

    State getState() const noexcept          { return state.load(); }
    float getProgress() const noexcept       { return progress.load(); }
    juce::String getMessage() const          { const juce::ScopedLock sl (lock); return message; }
    juce::String getLoadedName() const       { const juce::ScopedLock sl (lock); return loadedName; }

    // ---- audio thread ---------------------------------------------------------------
    // Returns true and fills 'out' when a newly loaded guitar is waiting.
    bool fetchIfChanged (GuitarSetPtr& out, int& seenVersion)
    {
        const int v = readyVersion.load (std::memory_order_acquire);
        if (v == seenVersion) return false;
        seenVersion = v;
        out = std::atomic_load (&ready);
        return true;
    }

    // audio thread hands the previous guitar back here; it is freed on the message thread
    void retire (GuitarSetPtr old) { std::atomic_store (&retired, std::move (old)); }

private:
    enum class Job { None, Download, InstallZip };

    void post (Job j, const juce::File& f)
    {
        const juce::ScopedLock sl (lock);
        pendingJob = j; pendingFile = f;
        notify();
    }

    void setStatus (State s, const juce::String& msg, float p = 0.0f)
    {
        state = s; progress = p;
        const juce::ScopedLock sl (lock);
        message = msg;
    }

    void timerCallback() override
    {
        std::atomic_store (&retired, GuitarSetPtr());   // free old guitars off the audio thread
    }

    void run() override
    {
        if (! isInstalled())
            setStatus (State::Missing, "Sound Set not installed");

        while (! threadShouldExit())
        {
            Job job; juce::File file; bool loadGuitar; juce::String guitarId;
            {
                const juce::ScopedLock sl (lock);
                job = pendingJob; file = pendingFile; pendingJob = Job::None;
                loadGuitar = guitarDirty; guitarId = desiredGuitar; guitarDirty = false;
            }

            if (job == Job::Download)   { if (download()) loadGuitar = true; }
            if (job == Job::InstallZip) { if (install (file)) loadGuitar = true; }
            if (loadGuitar && isInstalled() && guitarId.isNotEmpty()) load (guitarId);

            if (! threadShouldExit()) wait (-1);
        }
    }

    bool download()
    {
        const auto url = downloadUrl();
        if (url.isEmpty()) { setStatus (State::Error, "No download URL in this build - use Locate"); return false; }

        const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("DaliGTR-SoundSet.zip");
        tmp.deleteFile();
        setStatus (State::Downloading, "Downloading Sound Set...", 0.0f);

        int status = 0;
        auto stream = juce::URL (url).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                             .withConnectionTimeoutMs (20000)
                                                             .withNumRedirectsToFollow (10)
                                                             .withStatusCode (&status));
        if (stream == nullptr || (status != 0 && status != 200))
        {
            setStatus (State::Error, "Download failed (HTTP " + juce::String (status) + ")");
            return false;
        }

        const juce::int64 total = stream->getTotalLength();
        {
            juce::FileOutputStream out (tmp);
            if (! out.openedOk()) { setStatus (State::Error, "Cannot write " + tmp.getFullPathName()); return false; }
            juce::HeapBlock<char> buf (1 << 16);
            juce::int64 done = 0;
            while (! stream->isExhausted())
            {
                if (threadShouldExit()) return false;
                const int n = stream->read (buf.get(), 1 << 16);
                if (n <= 0) break;
                out.write (buf.get(), (size_t) n);
                done += n;
                if (total > 0) progress = (float) ((double) done / (double) total);
            }
            out.flush();
            if (total > 0 && done < total) { setStatus (State::Error, "Download interrupted"); return false; }
        }
        const bool ok = install (tmp);
        tmp.deleteFile();
        return ok;
    }

    bool install (const juce::File& zipFile)
    {
        setStatus (State::Installing, "Installing Sound Set...", 0.0f);
        juce::ZipFile zip (zipFile);
        if (zip.getNumEntries() == 0) { setStatus (State::Error, "Not a valid Sound Set zip"); return false; }

        const auto dest = soundSetFolder();
        const auto staging = dest.getSiblingFile ("Sounds.installing");
        staging.deleteRecursively();
        staging.createDirectory();
        const auto r = zip.uncompressTo (staging, true);
        if (r.failed() || ! staging.getChildFile ("soundset.json").existsAsFile())
        {
            staging.deleteRecursively();
            setStatus (State::Error, "Install failed: " + (r.failed() ? r.getErrorMessage() : juce::String ("soundset.json missing")));
            return false;
        }
        dest.deleteRecursively();
        staging.moveFileTo (dest);
        setStatus (State::Loading, "Sound Set installed", 1.0f);
        return true;
    }

    void load (const juce::String& id)
    {
        const auto index = juce::JSON::parse (soundSetFolder().getChildFile ("soundset.json"));
        juce::String folder, name;
        const auto list = index["guitars"];
        for (int i = 0; i < list.size(); ++i)
            if (list[i]["id"].toString() == id) { folder = list[i]["folder"].toString(); name = list[i]["name"].toString(); }

        if (folder.isEmpty())
        {
            setStatus (State::Error, "Guitar '" + id + "' is not in this Sound Set yet");
            return;
        }

        setStatus (State::Loading, "Loading " + name + "...", 0.0f);
        juce::String err;
        auto g = GuitarLoader::load (soundSetFolder().getChildFile (folder), err,
                                     [this] (float p) { progress = p; },
                                     [this] { return threadShouldExit() || guitarDirty.load(); });
        if (g == nullptr)
        {
            if (err != "aborted") setStatus (State::Error, err);
            return;
        }
        std::atomic_store (&ready, GuitarSetPtr (std::move (g)));
        readyVersion.fetch_add (1, std::memory_order_release);
        {
            const juce::ScopedLock sl (lock);
            loadedName = name;
        }
        setStatus (State::Ready, name + " ready", 1.0f);
    }

    juce::CriticalSection lock;
    Job pendingJob = Job::None;
    juce::File pendingFile;
    juce::String desiredGuitar, message, loadedName;
    std::atomic<bool> guitarDirty { false };
    std::atomic<State> state { State::Missing };
    std::atomic<float> progress { 0.0f };

    GuitarSetPtr ready, retired;
    std::atomic<int> readyVersion { 0 };
};
} // namespace dgtr
