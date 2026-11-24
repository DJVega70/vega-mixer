#pragma once
#include <JuceHeader.h>

// Converts recorded takes into lossless delivery formats (WAV / FLAC / AIFF).
struct ExportOptions
{
    enum Format { WAV = 0, FLAC, AIFF };
    Format format = WAV;
    int bits = 24;              // 16, 24 or 32 (32 = float, WAV only)
    bool normalise = false;     // peak-normalise to -0.3 dBFS
    bool dither = true;         // TPDF dither when going to 16-bit
};

class ExportJob : public juce::ThreadWithProgressWindow
{
public:
    ExportJob (juce::Array<juce::File> srcs, juce::File dest, ExportOptions o)
        : ThreadWithProgressWindow ("Exporting...", true, true), sources (std::move (srcs)), destDir (std::move (dest)), opts (o)
    {
        formats.registerBasicFormats();
    }

    juce::StringArray errors, written;

    void run() override
    {
        destDir.createDirectory();
        for (int i = 0; i < sources.size() && ! threadShouldExit(); ++i)
        {
            setStatusMessage ("Exporting " + sources[i].getFileName());
            exportOne (sources[i], i, sources.size());
        }
    }

    // Launched with launchThread(): reports the result, then deletes itself.
    void threadComplete (bool userPressedCancel) override
    {
        juce::String msg;
        if (userPressedCancel) msg = "Export cancelled.";
        else
        {
            msg << written.size() << " file(s) exported to:\n" << destDir.getFullPathName();
            if (! errors.isEmpty()) msg << "\n\nProblems:\n" << errors.joinIntoString ("\n");
        }
        const auto folder = destDir;
        const bool ok = ! userPressedCancel && ! written.isEmpty();
        juce::AlertWindow::showOkCancelBox (ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                                            "Vega Mixer Export", msg, ok ? "Open Folder" : "OK", "Close", nullptr,
                                            juce::ModalCallbackFunction::create ([folder, ok] (int r)
                                            { if (ok && r == 1) folder.startAsProcess(); }));
        delete this;
    }

    static juce::String extensionFor (ExportOptions::Format f)
    {
        return f == ExportOptions::FLAC ? ".flac" : (f == ExportOptions::AIFF ? ".aif" : ".wav");
    }

private:
    void exportOne (const juce::File& src, int index, int total)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (src));
        if (reader == nullptr) { errors.add ("Couldn't read " + src.getFileName()); return; }

        const int numCh = (int) reader->numChannels;
        const juce::int64 len = reader->lengthInSamples;

        float gain = 1.0f;
        if (opts.normalise && len > 0)
        {
            juce::Range<float> ranges[2];
            reader->readMaxLevels (0, len, ranges, juce::jmin (numCh, 2));
            float peak = 0.0f;
            for (int c = 0; c < juce::jmin (numCh, 2); ++c)
                peak = juce::jmax (peak, std::abs (ranges[c].getStart()), std::abs (ranges[c].getEnd()));
            if (peak > 1.0e-6f) gain = juce::Decibels::decibelsToGain (-0.3f) / peak;
        }

        std::unique_ptr<juce::AudioFormat> fmt;
        int bits = opts.bits;
        switch (opts.format)
        {
            case ExportOptions::FLAC: fmt = std::make_unique<juce::FlacAudioFormat>(); bits = juce::jmin (bits, 24); break;
            case ExportOptions::AIFF: fmt = std::make_unique<juce::AiffAudioFormat>(); bits = juce::jmin (bits, 24); break;
            case ExportOptions::WAV:
            default:                  fmt = std::make_unique<juce::WavAudioFormat>(); break;
        }

        auto out = destDir.getChildFile (src.getFileNameWithoutExtension() + extensionFor (opts.format)).getNonexistentSibling();
        std::unique_ptr<juce::OutputStream> os (out.createOutputStream());
        if (os == nullptr) { errors.add ("Couldn't create " + out.getFullPathName()); return; }

        std::unique_ptr<juce::AudioFormatWriter> writer (fmt->createWriterFor (os.get(), reader->sampleRate, (unsigned int) numCh,
                                                                              bits, {}, opts.format == ExportOptions::FLAC ? 5 : 0));
        if (writer == nullptr) { errors.add ("Format doesn't support these settings: " + out.getFileName()); return; }
        os.release();

        const bool addDither = opts.dither && bits == 16;
        const float lsb = 1.0f / 32768.0f;
        juce::Random rng;
        constexpr int block = 16384;
        juce::AudioBuffer<float> buf (numCh, block);

        for (juce::int64 pos = 0; pos < len && ! threadShouldExit(); pos += block)
        {
            const int n = (int) juce::jmin ((juce::int64) block, len - pos);
            reader->read (&buf, 0, n, pos, true, numCh > 1);
            if (gain != 1.0f) buf.applyGain (0, n, gain);
            if (addDither)
                for (int c = 0; c < numCh; ++c)
                {
                    auto* d = buf.getWritePointer (c);
                    for (int k = 0; k < n; ++k)
                        d[k] += (rng.nextFloat() - rng.nextFloat()) * lsb;
                }
            writer->writeFromAudioSampleBuffer (buf, 0, n);
            setProgress ((index + (double) (pos + n) / (double) juce::jmax ((juce::int64) 1, len)) / total);
        }
        writer.reset();
        written.add (out.getFullPathName());
    }

    juce::Array<juce::File> sources;
    juce::File destDir;
    ExportOptions opts;
    juce::AudioFormatManager formats;
};
