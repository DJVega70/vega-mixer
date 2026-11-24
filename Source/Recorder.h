#pragma once
#include <JuceHeader.h>
#include <array>

// Multitrack recorder: one 32-bit float WAV per armed channel + optional stereo mix.
// Writers are created/destroyed on the message thread; the audio thread only pushes samples.
template <int NumTracks>
class Recorder
{
public:
    Recorder()  { thread.startThread(); }
    ~Recorder() { stop(); thread.stopThread (2000); }

    struct TrackSpec { int index; int numChannels; juce::File file; };

    bool start (const std::vector<TrackSpec>& specs, double sampleRate, juce::String& error)
    {
        stop();
        std::array<std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter>, NumTracks> fresh;
        std::array<int, NumTracks> chans {};
        juce::WavAudioFormat wav;

        for (auto& s : specs)
        {
            s.file.getParentDirectory().createDirectory();
            s.file.deleteFile();
            std::unique_ptr<juce::OutputStream> os (s.file.createOutputStream());
            if (os == nullptr) { error = "Couldn't create " + s.file.getFullPathName(); return false; }

            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sampleRate, (unsigned int) s.numChannels,
                                                                            32, {}, 0));
            if (w == nullptr) { error = "Couldn't open a WAV writer"; return false; }
            os.release(); // writer owns the stream now

            fresh[(size_t) (unsigned) s.index].reset (new juce::AudioFormatWriter::ThreadedWriter (w.release(), thread, 1 << 17));
            chans[(size_t) (unsigned) s.index] = s.numChannels;
        }

        {
            const juce::ScopedLock sl (lock);
            writers = std::move (fresh);
            numChans = chans;
            samplesWritten = 0;
            active = true;
        }
        return true;
    }

    void stop()
    {
        std::array<std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter>, NumTracks> old;
        {
            const juce::ScopedLock sl (lock);
            active = false;
            old = std::move (writers);
        }
        // `old` is destroyed here, which flushes and closes the files.
    }

    bool isActive() const noexcept           { return active.load(); }
    int  channelsFor (int idx) const noexcept { return numChans[(size_t) idx]; }
    juce::int64 getSamplesWritten() const    { return samplesWritten.load(); }

    // audio thread
    void write (int idx, const float* const* data, int n) noexcept
    {
        const juce::ScopedLock sl (lock);
        if (active && writers[(size_t) idx] != nullptr)
            writers[(size_t) idx]->write (data, n);
    }

    void advance (int n) noexcept { if (active) samplesWritten += n; }

private:
    juce::TimeSliceThread thread { "Vega Recorder" };
    juce::CriticalSection lock;
    std::array<std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter>, NumTracks> writers;
    std::array<int, NumTracks> numChans {};
    std::atomic<bool> active { false };
    std::atomic<juce::int64> samplesWritten { 0 };
};
