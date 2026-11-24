#include <JuceHeader.h>
#include "MainComponent.h"

class VegaMixerApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Vega Productions Mixer"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override { mainWindow = std::make_unique<MainWindow> (getApplicationName()); }
    void shutdown() override                       { mainWindow = nullptr; }
    void systemRequestedQuit() override            { quit(); }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, VC::bg, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            setResizeLimits (640, 420, 4000, 2400);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (VegaMixerApplication)
