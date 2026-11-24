// Headless engine test: feeds sine/noise through every stage, records, checks output.
#include <JuceHeader.h>
#include "../Source/MixerEngine.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    MixerEngine e;
    const double sr = 48000.0;
    e.prepare (sr);

    auto& c1 = e.ch[0];
    c1.srcMode->set (1); c1.srcIndex->set (0);
    e.setChannelSlot (0, 1, modComp); c1.comp.thr->set (-30); c1.comp.ratio->set (8);
    e.setChannelSlot (0, 2, modGate);
    c1.eq.hfGain->set (6); c1.eq.lmfGain->set (-4);
    c1.conLo->set (3); c1.tape->set (1); c1.hpfOn->set (1);
    c1.sendDest->set (1); c1.sendLevel->set (0);
    c1.recArm->set (1);
    auto& c2 = e.ch[1];
    c2.srcMode->set (2); c2.srcIndex->set (0); c2.sendDest->set (2); c2.sendLevel->set (-6); c2.outDest->set (1);
    e.ch[kReverbStrip].recArm->set (1);
    e.master.tapeOn->set (1); e.master.tapeDrive->set (6);
    e.master.busOn->set (1); e.master.busThr->set (-20);
    e.setMasterSlot (2, mModEq); e.master.eq.hfGain->set (2);

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vegatest");
    dir.deleteRecursively();
    juce::StringArray files; juce::String err;
    if (! e.startRecording (dir, files, err)) { std::printf ("REC FAIL %s\n", err.toRawUTF8()); return 1; }

    const int block = 480, numOut = 4;
    juce::AudioBuffer<float> in (2, block), out (numOut, block);
    double ph = 0; juce::Random rng (1);
    float maxOut = 0, maxDirect = 0; bool nan = false;
    for (int b = 0; b < 200; ++b)   // 2 s
    {
        for (int k = 0; k < block; ++k)
        {
            const float v = 0.5f * (float) std::sin (ph) * (b < 100 ? 1.0f : 0.0f);
            ph += 2 * juce::MathConstants<double>::pi * 220.0 / sr;
            in.setSample (0, k, v); in.setSample (1, k, v * 0.5f + 0.01f * (rng.nextFloat() - 0.5f));
        }
        juce::AudioIODeviceCallbackContext ctx;
        e.audioDeviceIOCallbackWithContext (in.getArrayOfReadPointers(), 2, out.getArrayOfWritePointers(), numOut, block, ctx);
        for (int c = 0; c < numOut; ++c)
            for (int k = 0; k < block; ++k)
            {
                const float s = out.getSample (c, k);
                if (! std::isfinite (s)) nan = true;
                if (c < 2) maxOut = juce::jmax (maxOut, std::abs (s)); else maxDirect = juce::jmax (maxDirect, std::abs (s));
            }
    }
    e.stopRecording();
    juce::Thread::sleep (300);

    std::printf ("nan=%d maxOut=%.3f maxDirect(out3-4)=%.3f\n", (int) nan, maxOut, maxDirect);
    std::printf ("ch1 GR=%.1f dB  gate=%.1f  busGR=%.1f  vu=%.3f\n", e.meters[0].gr.load(), e.meters[0].gate.load(), e.masterMeter.gr.load(), e.meters[0].vu.load());
    std::printf ("reverb meter L=%.3f  delay meter L=%.3f\n", e.meters[kReverbStrip].l.load(), e.meters[kDelayStrip].l.load());
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    for (auto& f : files)
    {
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (f)));
        juce::Range<float> lv[2]; if (r) r->readMaxLevels (0, r->lengthInSamples, lv, (int) juce::jmin (2u, r->numChannels));
        std::printf ("%s: %s ch=%u len=%.2fs bits=%u peak=%.3f\n", juce::File (f).getFileName().toRawUTF8(), r ? "ok" : "UNREADABLE",
                     r ? r->numChannels : 0, r ? r->lengthInSamples / sr : 0.0, r ? r->bitsPerSample : 0, r ? lv[0].getEnd() : 0.0f);
    }
    // export path: FLAC 24 + WAV 16
    for (auto fmt : { 1, 0 })
    {
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (files[0])));
        std::unique_ptr<juce::AudioFormat> af; if (fmt == 1) af.reset (new juce::FlacAudioFormat()); else af.reset (new juce::WavAudioFormat());
        auto of = dir.getChildFile (fmt == 1 ? "x.flac" : "x.wav");
        auto* os = of.createOutputStream().release();
        std::unique_ptr<juce::AudioFormatWriter> w (af->createWriterFor (os, sr, r->numChannels, fmt == 1 ? 24 : 16, {}, fmt == 1 ? 5 : 0));
        w->writeFromAudioReader (*r, 0, -1); w.reset();
        std::printf ("export %s size=%lld\n", of.getFileName().toRawUTF8(), of.getSize());
    }
    return nan ? 1 : 0;
}
