#pragma once
#include "PluginProcessor.h"
#include "ToneBoyLookAndFeel.h"
#include "DotMatrixLCDComponent.h"

class PWMToneBoyAudioProcessorEditor final : public juce::AudioProcessorEditor,private juce::Timer
{
public:
    explicit PWMToneBoyAudioProcessorEditor(PWMToneBoyAudioProcessor&);
    ~PWMToneBoyAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshReadoutsForDiagnostics() { timerCallback(); }
private:
    class Panel final : public juce::Component
    {
    public:
        explicit Panel(PWMToneBoyAudioProcessorEditor& e):editor(e) {}
        void paint(juce::Graphics& g) override { editor.paintPanel(g); }
    private: PWMToneBoyAudioProcessorEditor& editor;
    };
    void paintPanel(juce::Graphics&);
    void paintStaticPanel(juce::Graphics&);
    juce::Image skin;
    std::array<float,9> lastReadouts{-999,-999,-999,-999,-999,-999,-999,-999,-999};
    void timerCallback() override;
    void updateRatioAttachment();
    void updateReadouts();
    const char* valueParameter(size_t index) const;
    std::array<juce::Label,5> valueLabels;
    std::array<juce::String,5> editingIds;
    class ModeButton final : public juce::Button
    {
    public: ModeButton():juce::Button("Dynamics mode") {}
        void paintButton(juce::Graphics&,bool,bool) override {}
    } ratioModeButton;
    int attachedMode{-1};
    void chooseFile();
    void setupKnob(juce::Slider&,const char*,bool,juce::Rectangle<int>,const juce::String&);
    void setupButton(juce::ToggleButton&,const char*,const char*,juce::Rectangle<int>,const juce::String&);
    PWMToneBoyAudioProcessor& p;
    ToneBoyLookAndFeel look;
    Panel panel;
    DotMatrixLCDComponent lcd;
    juce::Slider threshold,ratio,decay,clock,crush;
    juce::ToggleButton eq,hpf,link,bypass,pad,ki,pda,weight,phones,analog;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliders;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttons;
    juce::ComboBox testChoice;
    juce::TextButton loadButton{"WAV / AIFF"};
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tips{this,700};
    juce::String fileStatus;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PWMToneBoyAudioProcessorEditor)
};
