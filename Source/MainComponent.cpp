#include "MainComponent.h"

static constexpr int kTopBarH = 34;

juce::File MainComponent::stateFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Vega Productions").getChildFile ("Vega Mixer").getChildFile ("state.xml");
}
juce::File MainComponent::presetDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Vega Productions").getChildFile ("Vega Mixer").getChildFile ("Presets");
}
juce::File MainComponent::takesRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Vega Mixer").getChildFile ("Takes");
}

MainComponent::MainComponent()
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    loadState();

    const auto err = deviceManager.initialise (64, 64, savedDeviceState.get(), true);
    if (err.isNotEmpty()) DBG (err);
    deviceManager.addAudioCallback (&engine);
    deviceManager.addChangeListener (this);

    machine = std::make_unique<MachinePanel> (engine);
    console.addAndMakeVisible (*machine);
    for (int i = 0; i < kNumStrips; ++i)
        console.addAndMakeVisible (strips.add (new ChannelStrip (engine, i)));
    masterStrip = std::make_unique<MasterStrip> (engine);
    console.addAndMakeVisible (*masterStrip);

    machine->setBounds (0, 0, MachinePanel::W, ChannelStrip::H);
    for (int i = 0; i < kNumStrips; ++i) strips[i]->setTopLeftPosition (MachinePanel::W + i * ChannelStrip::W, 0);
    masterStrip->setTopLeftPosition (MachinePanel::W + kNumStrips * ChannelStrip::W, 0);
    console.setSize (MachinePanel::W + kNumStrips * ChannelStrip::W + MasterStrip::W, ChannelStrip::H);

    viewport.setViewedComponent (&console, false);
    viewport.setScrollBarsShown (true, true);
    viewport.setScrollBarThickness (10);
    addAndMakeVisible (viewport);

    // presets
    machine->presets.onChange = [this]
    {
        const int i = machine->presets.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (i, presetFiles.size())) loadPreset (presetFiles[i]);
    };
    machine->saveBtn.onClick   = [this] { savePreset(); };
    machine->renameBtn.onClick = [this] { renamePreset(); };
    machine->delBtn.onClick    = [this] { deletePreset(); };
    scanPresets (currentPreset);

    // top bar
    timeLabel.setFont (VegaFonts::mono (17.0f));
    timeLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff141414));
    timeLabel.setColour (juce::Label::textColourId, VC::cyan);
    timeLabel.setJustificationType (juce::Justification::centred);
    timeLabel.setText ("00:00:00.0", juce::dontSendNotification);
    timeLabel.setTooltip ("Record time");
    addAndMakeVisible (timeLabel);
    bpm = std::make_unique<BpmBox> (*engine.master.bpm);
    addAndMakeVisible (*bpm);

    for (auto* b : { &recBtn, &stopBtn, &exportBtn, &takesBtn, &setupBtn })
    {
        b->getProperties().set ("tab", true);
        addAndMakeVisible (b);
    }
    recBtn.getProperties().set ("led", VC::red.toString());
    recBtn.setTooltip ("Record armed channels and the mix");
    stopBtn.setTooltip ("Stop recording");
    exportBtn.setTooltip ("Export takes to WAV, FLAC or AIFF");
    takesBtn.setTooltip ("Open the folder with your recordings");
    setupBtn.setTooltip ("Audio device, inputs and outputs");
    recBtn.onClick    = [this] { toggleRecord(); };
    stopBtn.onClick   = [this] { stopRecord(); };
    exportBtn.onClick = [this] { showExport(); };
    takesBtn.onClick  = [] { takesRoot().createDirectory(); takesRoot().startAsProcess(); };
    setupBtn.onClick  = [this] { showAudioSetup(); };

    statusLabel.setFont (VegaFonts::label (12.0f));
    statusLabel.setColour (juce::Label::textColourId, VC::label);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (statusLabel);

    rebuildDevice();
    setZoom (zoom);
    setSize (juce::jmin (console.getWidth() + 12, 1600), ChannelStrip::H + kTopBarH + 12);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    engine.stopRecording();
    saveState();
    deviceManager.removeAudioCallback (&engine);
    deviceManager.removeChangeListener (this);
    deviceManager.closeAudioDevice();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (VC::bg);
    g.setColour (VC::topBar);
    g.fillRect (0, 0, getWidth(), kTopBarH);
    g.setColour (juce::Colours::black);
    g.drawHorizontalLine (kTopBarH - 1, 0, (float) getWidth());

    // logo: circled V
    const juce::Rectangle<float> ring (12.0f, 8.0f, 18.0f, 18.0f);
    g.setColour (VC::white);
    g.drawEllipse (ring, 1.4f);
    juce::Path v;
    v.startNewSubPath (ring.getX() + 5.0f, ring.getY() + 5.5f);
    v.lineTo (ring.getCentreX(), ring.getBottom() - 5.0f);
    v.lineTo (ring.getRight() - 5.0f, ring.getY() + 5.5f);
    g.strokePath (v, juce::PathStrokeType (1.5f));

    g.setFont (VegaFonts::label (17.0f));
    g.drawText ("Vega Productions", 38, 0, 140, kTopBarH, juce::Justification::centredLeft);
    g.setColour (VC::label);
    g.setFont (VegaFonts::label (11.0f));
    g.drawText ("MIXER", 160, 1, 60, kTopBarH, juce::Justification::centredLeft);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    auto bar = r.removeFromTop (kTopBarH).reduced (8, 6);
    for (auto* b : { &setupBtn, &takesBtn, &exportBtn, &stopBtn, &recBtn })
    {
        b->setBounds (bar.removeFromRight (b == &takesBtn || b == &exportBtn ? 60 : 50));
        bar.removeFromRight (6);
    }
    bar.removeFromRight (10);
    bpm->setBounds (bar.removeFromRight (104));
    bar.removeFromRight (12);
    timeLabel.setBounds (bar.removeFromRight (104));
    bar.removeFromRight (14);
    bar.removeFromLeft (220);
    statusLabel.setBounds (bar);
    viewport.setBounds (r);
}

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    if (k.getModifiers().isCommandDown())
    {
        if (k.getKeyCode() == '=' || k.getKeyCode() == '+') { setZoom (zoom + 0.1f); return true; }
        if (k.getKeyCode() == '-')                          { setZoom (zoom - 0.1f); return true; }
        if (k.getKeyCode() == '0')                          { setZoom (1.0f); return true; }
    }
    if (k == juce::KeyPress::spaceKey) { toggleRecord(); return true; }
    return false;
}

//==============================================================================
void MainComponent::timerCallback()
{
    machine->updateMeters();
    for (auto* s : strips) s->updateMeters();
    masterStrip->updateMeters();

    if (engine.recorder.isActive())
    {
        const double secs = (double) engine.recorder.getSamplesWritten() / engine.getSampleRate();
        const int h = (int) secs / 3600, m = ((int) secs / 60) % 60, s = (int) secs % 60, t = (int) (secs * 10.0) % 10;
        timeLabel.setText (juce::String::formatted ("%02d:%02d:%02d.%d", h, m, s, t), juce::dontSendNotification);
        if (++blinkCounter >= 15) { blinkCounter = 0; blink = ! blink; recBtn.setToggleState (blink, juce::dontSendNotification); }
    }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*) { rebuildDevice(); }

void MainComponent::rebuildDevice()
{
    juce::StringArray inputs;
    int numOuts = 2;
    juce::String status = "No audio device - click AUDIO";
    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        const auto names = dev->getInputChannelNames();
        const auto active = dev->getActiveInputChannels();
        for (int i = 0; i < names.size(); ++i)
            if (active[i]) inputs.add (names[i]);
        numOuts = dev->getActiveOutputChannels().countNumberOfSetBits();
        status = dev->getName() + "  |  " + juce::String ((int) dev->getCurrentSampleRate()) + " Hz  "
                 + juce::String (dev->getCurrentBufferSizeSamples()) + " smp  |  " + juce::String (inputs.size()) + " in / "
                 + juce::String (numOuts) + " out";
    }
    for (auto* s : strips) s->rebuildDevice (inputs, numOuts);
    masterStrip->rebuildDevice (numOuts);
    statusLabel.setText (status, juce::dontSendNotification);
}

void MainComponent::refreshAll()
{
    machine->refreshAll();
    for (auto* s : strips) s->refreshAll();
    masterStrip->refreshAll();
    bpm->refresh();
}

//==============================================================================
void MainComponent::toggleRecord()
{
    if (engine.recorder.isActive()) { stopRecord(); return; }

    const auto folder = takesRoot().getChildFile ("Take " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S"));
    juce::StringArray files;
    juce::String error;
    if (! engine.startRecording (folder, files, error))
    {
        recBtn.setToggleState (false, juce::dontSendNotification);
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Can't record", error);
        return;
    }
    lastTake = folder;
    blinkCounter = 0; blink = true;
    recBtn.setToggleState (true, juce::dontSendNotification);
    statusLabel.setText ("Recording " + juce::String (files.size()) + " track(s) to " + folder.getFileName(), juce::dontSendNotification);
}

void MainComponent::stopRecord()
{
    const bool was = engine.recorder.isActive();
    engine.stopRecording();
    recBtn.setToggleState (false, juce::dontSendNotification);
    if (was) statusLabel.setText ("Saved " + lastTake.getFileName() + " (32-bit float WAV) - EXPORT to convert", juce::dontSendNotification);
}

void MainComponent::showAudioSetup()
{
    auto* sel = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 64, 2, 64, false, false, false, false);
    sel->setSize (560, 560);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (sel);
    o.dialogTitle = "Audio Setup";
    o.dialogBackgroundColour = VC::panel;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

void MainComponent::showExport()
{
    if (engine.recorder.isActive()) stopRecord();
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new ExportPanel (lastTake));
    o.dialogTitle = "Export - lossless";
    o.dialogBackgroundColour = VC::panel;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

void MainComponent::setZoom (float z)
{
    zoom = juce::jlimit (0.6f, 1.6f, z);
    juce::Desktop::getInstance().setGlobalScaleFactor (zoom);
}

//==============================================================================
std::unique_ptr<juce::XmlElement> MainComponent::mixToXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("VEGAMIX");
    engine.params.writeTo (*xml);
    auto* n = xml->createNewChildElement ("NAMES");
    for (int i = 0; i < kNumStrips; ++i) n->setAttribute ("ch" + juce::String (i + 1), engine.names[(size_t) i]);
    return xml;
}

void MainComponent::mixFromXml (const juce::XmlElement& xml)
{
    engine.params.readFrom (xml);
    if (auto* n = xml.getChildByName ("NAMES"))
        for (int i = 0; i < kNumStrips; ++i)
            engine.names[(size_t) i] = n->getStringAttribute ("ch" + juce::String (i + 1), engine.names[(size_t) i]);
}

void MainComponent::scanPresets (const juce::String& select)
{
    auto dir = presetDir();
    if (! dir.isDirectory())
    {
        // factory presets on first run
        dir.createDirectory();
        MixerEngine tmp;
        auto write = [&] (const juce::String& name)
        {
            auto xml = std::make_unique<juce::XmlElement> ("VEGAMIX");
            tmp.params.writeTo (*xml);
            xml->writeTo (dir.getChildFile (name + ".xml"));
        };
        write ("Init");
        tmp.master.tapeOn->set (1); tmp.master.tapeIps->set (0); tmp.master.tapeDrive->set (3);
        tmp.master.busOn->set (1); tmp.master.busThr->set (-12); tmp.master.busRatio->set (0); tmp.master.busRel->set (4);
        write ("Warm Bus - 15 ips");
        tmp.master.tapeIps->set (1); tmp.master.tapeDrive->set (0); tmp.master.busRatio->set (1);
        write ("Clean Bus - 30 ips");
    }

    presetFiles = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    presetFiles.sort();
    machine->presets.clear (juce::dontSendNotification);
    for (int i = 0; i < presetFiles.size(); ++i)
    {
        machine->presets.addItem (presetFiles[i].getFileNameWithoutExtension(), i + 1);
        if (presetFiles[i].getFileNameWithoutExtension() == select) machine->presets.setSelectedId (i + 1, juce::dontSendNotification);
    }
}

void MainComponent::loadPreset (const juce::File& f)
{
    if (auto xml = juce::XmlDocument::parse (f))
    {
        mixFromXml (*xml);
        currentPreset = f.getFileNameWithoutExtension();
        refreshAll();
    }
}

void MainComponent::savePreset()
{
    auto* aw = new juce::AlertWindow ("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", currentPreset.isEmpty() ? "My Mix" : currentPreset);
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
    {
        const auto name = juce::File::createLegalFileName (aw->getTextEditorContents ("name").trim());
        if (r != 1 || name.isEmpty()) return;
        presetDir().createDirectory();
        mixToXml()->writeTo (presetDir().getChildFile (name + ".xml"));
        currentPreset = name;
        scanPresets (name);
    }), true);
}

void MainComponent::renamePreset()
{
    const int i = machine->presets.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow (i, presetFiles.size())) return;
    const auto file = presetFiles[i];
    auto* aw = new juce::AlertWindow ("Rename preset", "New name:", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", file.getFileNameWithoutExtension());
    aw->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, file] (int r)
    {
        const auto name = juce::File::createLegalFileName (aw->getTextEditorContents ("name").trim());
        if (r != 1 || name.isEmpty()) return;
        file.moveFileTo (file.getSiblingFile (name + ".xml"));
        currentPreset = name;
        scanPresets (name);
    }), true);
}

void MainComponent::deletePreset()
{
    const int i = machine->presets.getSelectedId() - 1;
    if (! juce::isPositiveAndBelow (i, presetFiles.size())) return;
    const auto file = presetFiles[i];
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Delete preset",
                                        "Delete \"" + file.getFileNameWithoutExtension() + "\"?", "Delete", "Cancel", nullptr,
                                        juce::ModalCallbackFunction::create ([this, file] (int r)
                                        {
                                            if (r != 1) return;
                                            file.deleteFile();
                                            currentPreset = {};
                                            scanPresets();
                                        }));
}

//==============================================================================
void MainComponent::loadState()
{
    auto xml = juce::XmlDocument::parse (stateFile());
    if (xml == nullptr) return;
    mixFromXml (*xml);
    if (auto* d = xml->getChildByName ("DEVICESETUP"))
        savedDeviceState = std::make_unique<juce::XmlElement> (*d);
    zoom = (float) xml->getDoubleAttribute ("zoom", 1.0);
    lastTake = juce::File (xml->getStringAttribute ("lastTake"));
    currentPreset = xml->getStringAttribute ("preset");
}

void MainComponent::saveState()
{
    auto xml = mixToXml();
    xml->setAttribute ("zoom", zoom);
    xml->setAttribute ("lastTake", lastTake.getFullPathName());
    xml->setAttribute ("preset", currentPreset);
    if (auto dev = deviceManager.createStateXml())
        xml->addChildElement (dev.release());
    stateFile().getParentDirectory().createDirectory();
    xml->writeTo (stateFile());
}
