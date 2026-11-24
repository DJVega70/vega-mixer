#pragma once
#include <memory>
#include <string>

// Captures what Windows is playing (YouTube, Spotify, browser...) as a stereo input.
// On Windows 10 2004+ it uses process loopback and EXCLUDES this app's own output,
// so monitoring through the same speakers doesn't feed back. Older Windows falls back
// to classic endpoint loopback (which includes the mixer's output - mute monitor then).
// On other platforms this is a stub that delivers silence.
class SystemAudioCapture
{
public:
    SystemAudioCapture();
    ~SystemAudioCapture();

    bool start (double sampleRate);   // message thread
    void stop();                      // message thread
    bool isRunning() const;
    double runningRate() const;

    // Audio thread: fills n frames (zero-padded if not enough audio has arrived).
    void read (float* left, float* right, int n) noexcept;

    // Human-readable state for the status bar.
    std::string status() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
