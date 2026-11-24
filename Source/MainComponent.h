#pragma once
#include "Strips.h"
#include "Exporter.h"

//==============================================================================
class ExportPanel : public juce::Component
{
public:
    explicit ExportPanel (juce::File lastTakeFolder) : takeFolder (std::move (lastTakeFolder))
    {
        for (auto* l : { &srcTitle, &fmtTitle, &bitsTitle })
        {
            l->setFont (VegaFonts::label (13.0f));
            l->setColour (juce::Label::textColourId, VC::label);
            addAndMakeVisible (l);
        }
        srcTitle.setText ("SOURCE", juce::dontSendNotification);
        fmtTitle.setText ("FORMAT", juce::dontSendNotification);
        bitsTitle.setText ("BIT DEPTH", juce::dontSendNotification);

        srcInfo.setFont (VegaFonts::mono (13.0f));
        srcInfo.setColour (juce::Label::textColourId, VC::cyan);
        srcInfo.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (srcInfo);

        chooseBtn.getProperties().set ("tab", true);
        chooseBtn.onClick = [this] { chooseSources(); };
        addAndMakeVisible (chooseBtn);

        format.addItemList ({ "WAV", "FLAC", "AIFF" }, 1);
        format.setSelectedId (1);
        format.onChange = [this] { updateBits(); };
        addAndMakeVisible (format);
        addAndMakeVisible (bits);
        updateBits();

        normalise.setButtonText ("Normalise to -0.3 dBFS");
        dither.setButtonText ("TPDF dither when exporting 16-bit");
        dither.setToggleState (true, juce::dontSendNotification);
        addAndMakeVisible (normalise);
        addAndMakeVisible (dither);

        exportBtn.getProperties().set ("tab", true);
        exportBtn.getProperties().set ("led", VC::cyan.toString());
        exportBtn.getProperties().set ("textColour", juce::Colour (0xff111111).toString());
        exportBtn.setToggleState (true, juce::dontSendNotification);
        exportBtn.onClick = [this] { startExport(); };
        addAndMakeVisible (exportBtn);

        if (takeFolder.isDirectory())
            for (auto& f : takeFolder.findChildFiles (juce::File::findFiles, false, "*.wav"))
                sources.add (f);
        updateSourceInfo();
        setSize (480, 310);
    }

    void paint (juce::Graphics& g) override { g.fillAll (VC::panel); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (18);
        srcTitle.setBounds (r.removeFromTop (20));
        auto srcRow = r.removeFromTop (56);
        chooseBtn.setBounds (srcRow.removeFromRight (120).withHeight (24));
        srcInfo.setBounds (srcRow.withTrimmedRight (8));
        r.removeFromTop (8);
        auto row = r.removeFromTop (20);
        fmtTitle.setBounds (row.removeFromLeft (row.getWidth() / 2));
        bitsTitle.setBounds (row);
        row = r.removeFromTop (24);
        format.setBounds (row.removeFromLeft (row.getWidth() / 2).withTrimmedRight (8));
        bits.setBounds (row);
        r.removeFromTop (12);
        normalise.setBounds (r.removeFromTop (24));
        dither.setBounds (r.removeFromTop (24));
        exportBtn.setBounds (r.removeFromBottom (30).withSizeKeepingCentre (180, 30));
    }

private:
    void updateBits()
    {
        const int prev = bits.getSelectedId();
        bits.clear (juce::dontSendNotification);
        bits.addItem ("16-bit", 16);
        bits.addItem ("24-bit", 24);
        if (format.getSelectedId() == 1) bits.addItem ("32-bit float", 32);
        bits.setSelectedId (bits.indexOfItemId (prev) >= 0 ? prev : 24, juce::dontSendNotification);
    }

    void updateSourceInfo()
    {
        if (sources.isEmpty()) { srcInfo.setText ("No files yet. Record a take, or choose files.", juce::dontSendNotification); return; }
        juce::StringArray n;
        for (auto& f : sources) n.add (f.getFileName());
        srcInfo.setText (juce::String (sources.size()) + " file(s): " + n.joinIntoString (", "), juce::dontSendNotification);
    }

    void chooseSources()
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose recordings to export",
                                                       takeFolder.exists() ? takeFolder : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                       "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResults().isEmpty()) return;
                                  sources = fc.getResults();
                                  updateSourceInfo();
                              });
    }

    void startExport()
    {
        if (sources.isEmpty()) { chooseSources(); return; }
        auto defaultDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Vega Mixer").getChildFile ("Exports");
        defaultDir.createDirectory();
        chooser = std::make_unique<juce::FileChooser> ("Choose a folder to export into", defaultDir);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto dir = fc.getResult();
                                  if (dir == juce::File()) return;
                                  ExportOptions o;
                                  o.format = (ExportOptions::Format) (format.getSelectedId() - 1);
                                  o.bits = bits.getSelectedId();
                                  o.normalise = normalise.getToggleState();
                                  o.dither = dither.getToggleState();
                                  (new ExportJob (sources, dir, o))->launchThread();
                                  if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                                      dw->exitModalState (0);
                              });
    }

    juce::File takeFolder;
    juce::Array<juce::File> sources;
    juce::Label srcTitle, fmtTitle, bitsTitle, srcInfo;
    juce::TextButton chooseBtn { "CHOOSE FILES" }, exportBtn { "EXPORT" };
    juce::ComboBox format, bits;
    juce::ToggleButton normalise, dither;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
// "124.0 BPM" display: drag up/down to change, double-click to type.
class BpmBox : public juce::Label
{
public:
    explicit BpmBox (Param& p) : param (p)
    {
        setFont (VegaFonts::mono (15.0f));
        setJustificationType (juce::Justification::centred);
        setColour (juce::Label::backgroundColourId, juce::Colour (0xff141414));
        setColour (juce::Label::textColourId, VC::text);
        setEditable (false, true);
        setTooltip ("Tempo for the delay. Drag to change, double-click to type.");
        onTextChange = [this] { param.set (getText().getFloatValue()); refresh(); };
        refresh();
    }
    void refresh() { setText (juce::String (param.get(), 1) + " BPM", juce::dontSendNotification); }
    void mouseDown (const juce::MouseEvent& e) override { startBpm = param.get(); juce::Label::mouseDown (e); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float step = e.mods.isShiftDown() ? 0.02f : 0.2f;
        param.set (std::round ((startBpm - (float) e.getDistanceFromDragStartY() * step) * 10.0f) / 10.0f);
        refresh();
    }
private:
    Param& param;
    float startBpm = 120.0f;
};

//==============================================================================
class MainComponent : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void toggleRecord();
    void stopRecord();
    void showAudioSetup();
    void showExport();
    void setZoom (float z);
    void rebuildDevice();
    void rebuildRateBox();
    void rateChosen();
    void switchToExclusive (double wantedRate);
    void refreshAll();

    // presets
    static juce::File presetDir();
    void scanPresets (const juce::String& select = {});
    void loadPreset (const juce::File&);
    void savePreset();
    void renamePreset();
    void deletePreset();
    std::unique_ptr<juce::XmlElement> mixToXml() const;
    void mixFromXml (const juce::XmlElement&);

    static juce::File stateFile();
    static juce::File takesRoot();
    void loadState();
    void saveState();

    VegaLookAndFeel lnf;
    juce::AudioDeviceManager deviceManager;
    MixerEngine engine;
    std::unique_ptr<juce::XmlElement> savedDeviceState;

    juce::Viewport viewport;
    juce::Component console;
    std::unique_ptr<MachinePanel> machine;
    juce::OwnedArray<ChannelStrip> strips;
    std::unique_ptr<MasterStrip> masterStrip;

    juce::Label timeLabel;
    std::unique_ptr<BpmBox> bpm;
    juce::ComboBox rateBox;
    juce::Array<double> rateList;
    juce::TextButton recBtn { "REC" }, stopBtn { "STOP" }, exportBtn { "EXPORT" }, takesBtn { "TAKES" }, setupBtn { "AUDIO" };
    juce::Label statusLabel;
    juce::TooltipWindow tooltips { this, 700 };

    juce::Array<juce::File> presetFiles;
    juce::File lastTake;
    juce::String currentPreset, deviceStatus;
    int sysCheck = 0;
    void manageSystemAudio();
    float zoom = 1.0f;
    int blinkCounter = 0;
    bool blink = false;
};
