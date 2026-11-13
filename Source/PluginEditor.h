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
private:
    class Panel final : public juce::Component
    {
    public:
        explicit Panel(PWMToneBoyAudioProcessorEditor& e):editor(e) {}
        void paint(juce::Graphics& g) override { editor.paintPanel(g); }
    private: PWMToneBoyAudioProcessorEditor& editor;
    };
    void paintPanel(juce::Graphics&);
    void timerCallback() override;
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
