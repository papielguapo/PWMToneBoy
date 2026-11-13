#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>
#include <cmath>
#include <cstdlib>

namespace {
int failures=0, checks=0;
void check(bool condition,const char* description)
{
    ++checks; if(!condition) { ++failures; std::cerr<<"FAIL: "<<description<<'\n'; }
}
void set(PWMToneBoyAudioProcessor& p,const char* id,float v)
{
    auto* a=p.apvts.getParameter(id); a->setValueNotifyingHost(a->convertTo0to1(v));
}
void finiteBuffer(const juce::AudioBuffer<float>& b)
{
    bool finite=true,bounded=true;
    for(int c=0;c<b.getNumChannels();++c) for(int n=0;n<b.getNumSamples();++n) { const auto x=b.getSample(c,n); finite &= std::isfinite(x); bounded &= std::abs(x)<2.0f; }
    check(finite,"finite audio"); check(bounded,"bounded audio");
}
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::MidiBuffer midi;
    PWMToneBoyAudioProcessor p;
    check(p.getParameters().size()==14,"exactly fourteen parameters");
    check(std::abs(p.parameter("threshold")+18)<0.001f && p.parameter("ratioIndex")==2 && p.parameter("decayIndex")==1,"parameter defaults");
    check(p.getBypassParameter()==p.apvts.getParameter("bypass"),"host bypass mapping");
    for(float r:PWMToneBoyAudioProcessor::ratios)
    {
        float previous=1;
        for(int i=0;i<500;++i) { const auto g=p.computeSCurveGainLin(juce::Decibels::decibelsToGain(-70.0f+float(i)*0.2f),-18,r); check(g<=previous+1e-6f && g>0 && g<=1,"monotonic S-curve"); previous=g; }
        const auto knee=juce::jlimit(2.5f,10.5f,11-r*0.45f);
        for(float delta:{-knee/2,knee/2}) { const auto lo=p.computeSCurveGainLin(juce::Decibels::decibelsToGain(-18+delta-0.0001f),-18,r); const auto hi=p.computeSCurveGainLin(juce::Decibels::decibelsToGain(-18+delta+0.0001f),-18,r); check(std::abs(lo-hi)<0.0001f,"continuous knee joins"); }
    }
    for(double sr:{32000.0,44100.0,48000.0,96000.0,192000.0})
    {
        p.prepareToPlay(sr,128);
        juce::AudioBuffer<float> silence(2,128); silence.clear();
        for(int i=0;i<8;++i) p.processBlock(silence,midi);
        check(silence.getMagnitude(0,128)==0,"bit-exact silence from rest");
        for(int block:{1,17,127,128,511,4097})
        {
            juce::AudioBuffer<float> audio(2,block);
            for(int c=0;c<2;++c) for(int n=0;n<block;++n) audio.setSample(c,n,float(std::sin(n*0.17+c)*0.95));
            p.processBlock(audio,midi); finiteBuffer(audio);
        }
        set(p,"threshold",-42); set(p,"ratioIndex",6); set(p,"dacCrush",100); set(p,"clockRate",4);
        for(int k=0;k<24;++k)
        {
            for(const char* id:{"eqIn","hpfIn","linkAutoGain","kiMode","pdaMode","kWeightSc","phonesRawMode","pad"}) set(p,id,float(k%2));
            juce::AudioBuffer<float> audio(2,257);
            for(int n=0;n<257;++n) { audio.setSample(0,n,float(std::sin((n+k*257)*0.023)*1.5)); audio.setSample(1,n,n%63==0?1.0f:0.0f); }
            p.processBlock(audio,midi); finiteBuffer(audio);
        }
        p.releaseResources();
    }
    // A settled bypass must reproduce the latency-shifted input exactly.
    p.prepareToPlay(48000,128); set(p,"bypass",1);
    juce::AudioBuffer<float> dry(2,128); dry.clear();
    for(int k=0;k<16;++k) p.processBlock(dry,midi);
    dry.clear(); dry.setSample(0,0,0.75f); dry.setSample(1,0,-0.5f); p.processBlock(dry,midi);
    check(dry.getSample(0,p.getLatencySamples())==0.75f && dry.getSample(1,p.getLatencySamples())==-0.5f,"exact delayed bypass");
    set(p,"bypass",0); set(p,"threshold",-25.4f); p.editorWidth.store(630);
    juce::MemoryBlock state; p.getStateInformation(state);
    PWMToneBoyAudioProcessor restored; restored.setStateInformation(state.getData(),int(state.getSize()));
    check(std::abs(restored.parameter("threshold")+25.4f)<0.01f && restored.editorWidth.load()==630,"state and window size roundtrip");
    const char bad[]{'b','a','d'}; restored.setStateInformation(bad,3); check(std::abs(restored.parameter("threshold")+25.4f)<0.01f,"corrupt state rejected");
    auto layout=p.getBusesLayout(); layout.inputBuses.set(0,juce::AudioChannelSet::mono()); layout.outputBuses.set(0,juce::AudioChannelSet::mono());
    check(p.setBusesLayout(layout),"mono layout accepted"); p.prepareToPlay(48000,64);
    juce::AudioBuffer<float> mono(1,1000); mono.clear(); mono.setSample(0,0,0.9f); p.processBlock(mono,midi); finiteBuffer(mono);
    p.reset(); mono.clear(); p.processBlock(mono,midi); check(mono.getMagnitude(0,1000)==0,"reset clears all audio history");
    // Settling after real signal, especially the very low poles at 4x/192 kHz.
    set(p,"decayIndex",0); set(p,"bypass",0);
    p.prepareToPlay(192000,1024); mono.setSize(1,1024); mono.clear(); mono.setSample(0,0,1);
    p.processBlock(mono,midi);
    for(int k=0;k<1500;++k) { mono.clear(); p.processBlock(mono,midi); }
    check(mono.getMagnitude(0,1024)==0,"high sample-rate tail settles to exact silence");
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
    PWMToneBoyAudioProcessor demo;
    juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined);
    demo.prepareToPlay(48000,256);
    for(int source:{1,2,3}) { demo.setTestSource(source); juce::AudioBuffer<float> audio(2,256); audio.clear(); demo.processBlock(audio,midi); check(audio.getMagnitude(0,256)>0,"standalone loop produces audio"); finiteBuffer(audio); }
    check(!demo.loadTestFile(juce::File::getCurrentWorkingDirectory().getChildFile("missing-test-file.wav")),"unreadable file handled");
    // Exercise actual editor rendering at minimum, reference and maximum sizes.
    for(auto* param:restored.getParameters()) param->setValueNotifyingHost(param->getDefaultValue());
    std::unique_ptr<juce::AudioProcessorEditor> editor(restored.createEditor());
    editor->setVisible(true);
    for(int w:{336,420,630,840})
    {
        editor->setSize(w,juce::roundToInt(float(w)*680/420));
        const float scale=float(w)/420;
        auto* hit=editor->getComponentAt(juce::roundToInt(104*scale),juce::roundToInt(420*scale));
        check(hit!=nullptr && hit->getName()=="PWM Threshold","scaled threshold hit target matches drawing");
        auto image=editor->createComponentSnapshot(editor->getLocalBounds()); check(image.isValid(),"resized editor snapshot");
        if(argc>1) { juce::File out=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).getChildFile("UI-"+juce::String(w)+".png"); out.getParentDirectory().createDirectory(); juce::FileOutputStream stream(out); stream.setPosition(0); stream.truncate(); juce::PNGImageFormat png; png.writeImageToStream(image,stream); }
    }
    std::cout<<checks<<" checks, "<<failures<<" failures. Latency: "<<p.getLatencySamples()<<" samples.\n";
    return failures==0?EXIT_SUCCESS:EXIT_FAILURE;
}
