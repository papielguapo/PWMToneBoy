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
struct Spectrum { double thdDb{}, h2Db{}, h3Db{}, imdDb{}, gainDb{}, modelDb{}; };
Spectrum measure(bool ki, bool compressed=false, bool twoTone=false, float frequency=1000,bool analog=false,float amplitude=-1,int ratioOverride=-1,bool standardDetector=true)
{
    auto processor=std::make_unique<PWMToneBoyAudioProcessor>();
    set(*processor,"analogMode",analog?1.0f:0.0f);
    set(*processor,"kiMode",ki?1.0f:0.0f); set(*processor,"eqIn",0);
    set(*processor,"ratioIndex",ratioOverride>=0?float(ratioOverride):(compressed?2.0f:0.0f)); set(*processor,"threshold",compressed?-18.0f:0.0f);
    if(!standardDetector) { set(*processor,"pdaMode",0); set(*processor,"kWeightSc",0); set(*processor,"linkAutoGain",0); set(*processor,"decayIndex",0); }
    const double peak=amplitude>=0?amplitude:(compressed?0.5:0.2);
    processor->prepareToPlay(48000,256);
    juce::AudioBuffer<float> buffer(2,256); juce::MidiBuffer midi;
    std::array<double,13> real{},imag{}; double imReal=0,imImag=0;
    const double pi=juce::MathConstants<double>::twoPi;
    for(int start=0;start<96000;start+=256)
    {
        const int count=juce::jmin(256,96000-start); buffer.setSize(2,count,false,false,true);
        for(int n=0;n<count;++n)
        {
            const double t=double(start+n)/48000;
            const float value=twoTone?float(0.15*(std::sin(pi*5000*t)+std::sin(pi*6000*t))):float(peak*std::sin(pi*frequency*t));
            buffer.setSample(0,n,value); buffer.setSample(1,n,value);
        }
        processor->processBlock(buffer,midi);
        for(int n=0;n<count;++n) if(start+n>=48000)
        {
            const double t=double(start+n)/48000, value=buffer.getSample(0,n);
            for(int h=1;h<=12;++h) { const double angle=pi*(twoTone?5000:frequency)*h*t; real[size_t(h)]+=value*std::cos(angle); imag[size_t(h)]+=value*std::sin(angle); }
            imReal+=value*std::cos(pi*1000*t); imImag+=value*std::sin(pi*1000*t);
        }
    }
    const auto power=[&](int h){return real[size_t(h)]*real[size_t(h)]+imag[size_t(h)]*imag[size_t(h)];};
    const double fundamental=power(1); double distortion=0;
    for(int h=2;h<=12;++h) if(double(h)*frequency<24000 && !twoTone) distortion+=power(h);
    const auto relativeDb=[&](double energy){return 10*std::log10(juce::jmax(1.0e-20,energy/fundamental));};
    return {relativeDb(distortion),relativeDb(power(2)),relativeDb(power(3)),relativeDb(imReal*imReal+imImag*imImag),20*std::log10(std::sqrt(fundamental)*2/48000/peak),processor->linearResponseDb(frequency)};
}
float recovery(bool sustained)
{
    auto processor=std::make_unique<PWMToneBoyAudioProcessor>(); set(*processor,"decayIndex",1);
    processor->prepareToPlay(48000,256); juce::AudioBuffer<float> buffer(2,256); juce::MidiBuffer midi;
    const int duration=sustained?96000:960;
    for(int start=0;start<duration;start+=256) { buffer.clear(); for(int c=0;c<2;++c) for(int n=0;n<juce::jmin(256,duration-start);++n) buffer.setSample(c,n,0.6f); processor->processBlock(buffer,midi); }
    for(int n=0;n<38;++n) { buffer.clear(); processor->processBlock(buffer,midi); }
    return processor->reductionDb.load();
}

}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::MidiBuffer midi;
    PWMToneBoyAudioProcessor p;
    check(p.getParameters().size()==16,"sixteen parameters including output and A/D");
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
        set(p,"analogMode",0); p.prepareToPlay(sr,128);
        juce::AudioBuffer<float> silence(2,128); silence.clear();
        for(int i=0;i<8;++i) p.processBlock(silence,midi);
        check(silence.getMagnitude(0,128)==0,"digital D is bit-exact silent across sample rates");
        set(p,"analogMode",1); p.prepareToPlay(sr,128);
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
            for(const char* id:{"eqIn","hpfIn","linkAutoGain","kiMode","pdaMode","kWeightSc","phonesRawMode","pad","analogMode"}) set(p,id,float(k%2));
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
    auto neutralPtr=std::make_unique<PWMToneBoyAudioProcessor>(), legacyPtr=std::make_unique<PWMToneBoyAudioProcessor>(), quieterPtr=std::make_unique<PWMToneBoyAudioProcessor>(); auto& neutral=*neutralPtr; auto& legacy=*legacyPtr; auto& quieter=*quieterPtr;
    set(legacy,"dacCrush",100); set(legacy,"phonesRawMode",1); set(quieter,"outputGain",-6);
    neutral.prepareToPlay(48000,256); legacy.prepareToPlay(48000,256); quieter.prepareToPlay(48000,256);
    juce::AudioBuffer<float> a(2,256), b(2,256), q(2,256);
    for(int c=0;c<2;++c) for(int n=0;n<256;++n) a.setSample(c,n,0.2f*std::sin(float(n)*0.07f));
    b.makeCopyOf(a); q.makeCopyOf(a);
    neutral.processBlock(a,midi); legacy.processBlock(b,midi); quieter.processBlock(q,midi);
    bool identical=true; for(int n=0;n<256;++n) identical &= a.getSample(0,n)==b.getSample(0,n);
    check(identical,"legacy DAC and ProSound automation cannot enable processing");
    check(std::abs(q.getMagnitude(0,256)/a.getMagnitude(0,256)-juce::Decibels::decibelsToGain(-6.0f))<0.001f,"output trim applies calibrated dB gain");
    check(PWMToneBoyAudioProcessor::processCraneTriodeEven(0.4f,0.2f,0,1)==0.4f,"KI-off transfer is exactly linear");
    float ceilingState=0;
    check(PWMToneBoyAudioProcessor::processPhoenixSapphireCeiling(0.8f,ceilingState)==0.8f,"safety ceiling transparent below knee");
    check(PWMToneBoyAudioProcessor::processPhoenixSapphireCeiling(10.0f,ceilingState)<=1.0f,"safety ceiling bounded");
    const auto clean=measure(false), coloured=measure(true), compressed=measure(false,true), bass=measure(false,true,false,50), imdClean=measure(false,false,true), imdKi=measure(true,false,true);
    check(clean.thdDb < -100,"digital D is transparent with KI off at normal level");
    check(coloured.h2Db>clean.h2Db+20 && coloured.h2Db>coloured.h3Db+12,"KI introduces predominantly second harmonic");
    check(compressed.thdDb < -35,"moderate compression avoids excessive harmonic distortion");
    check(imdClean.imdDb < -100 && imdKi.imdDb < -40,"two-tone intermodulation remains controlled");
    check(std::abs(coloured.thdDb-(-62.0278))<0.2,"digital D with KI restores the v1.1 clean harmonic reference");
    check(std::abs(PWMToneBoyAudioProcessor::linearResponseDb(65,48000,false,0,1,0,false,false,1)-PWMToneBoyAudioProcessor::linearResponseDb(65,48000,false,0,1,0,false,false,0))<1e-9,"D ignores analog load instead of silently adding colour");
    const auto screenshotCase=measure(false,false,false,250,false,1.0f,1,false);
    const auto screenshotMinus6=measure(false,false,false,250,false,0.5011872f,1,false);
    std::cout<<"Screenshot regression D: 250Hz/0dBFS/2:1/KI-PDA-HSC-off THD="<<screenshotCase.thdDb<<"dB; -6dBFS="<<screenshotMinus6.thdDb<<"dB\n";
    check(screenshotCase.thdDb < -75,"full-scale 250 Hz screenshot setup stays clean in digital D");
    check(screenshotMinus6.thdDb < -100,"250 Hz digital D is transparent below compression knee");
    const float shortRecovery=recovery(false), longRecovery=recovery(true);
    check(longRecovery>shortRecovery+0.5f,"sustained program leaves longer release memory");
    std::cout<<"THD dB: clean="<<clean.thdDb<<", KI="<<coloured.thdDb<<", compressed="<<compressed.thdDb<<", bass50Hz="<<bass.thdDb<<"; KI H2/H3="<<coloured.h2Db<<"/"<<coloured.h3Db<<"; IMD 5+6kHz="<<imdClean.imdDb<<"/"<<imdKi.imdDb<<"; recovery short/long="<<shortRecovery<<"/"<<longRecovery<<" dB\n";
    auto testInput=std::make_unique<std::array<float,8192>>(), testOutput=std::make_unique<std::array<float,8192>>();
    auto analysis=ToneBoyHarmonicAnalysis::analyse(*testInput,*testOutput,48000);
    check(analysis.status==ToneBoyHarmonicResult::Status::silence,"harmonic view does not invent a tone in silence");
    const double tau=juce::MathConstants<double>::twoPi;
    for(int n=0;n<8192;++n)
    {
        const auto phase=tau*997.3*n/48000;
        (*testInput)[size_t(n)]=float(0.2*std::sin(phase+0.17));
        (*testOutput)[size_t(n)]=float(0.03+0.2*std::sin(phase-0.42)+0.0002*std::sin(2*phase)+0.00002*std::sin(3*phase));
    }
    analysis=ToneBoyHarmonicAnalysis::analyse(*testInput,*testOutput,48000);
    check(analysis.status==ToneBoyHarmonicResult::Status::valid && std::abs(analysis.frequency-997.3)<0.02,"off-bin fundamental estimated from input");
    check(std::abs(analysis.harmonicsDb[1]+60)<1 && std::abs(analysis.harmonicsDb[2]+80)<3,"known H2/H3 measured relative to H1 despite phase and DC");
    check(std::abs(analysis.thdDb+59.9568)<1,"harmonic THD matches known synthetic reference");
    std::cout<<"Harmonic reference f0="<<analysis.frequency<<" THD="<<analysis.thdDb<<" THD+N="<<analysis.thdnDb<<"\n";
    check(std::abs(analysis.thdnDb-analysis.thdDb)<1,"noise-free THD+N agrees with known harmonic distortion");
    for(int n=0;n<8192;++n) (*testOutput)[size_t(n)]+=float(0.001*std::sin(tau*6000*n/48000));
    analysis=ToneBoyHarmonicAnalysis::analyse(*testInput,*testOutput,48000);
    check(analysis.thdnDb>analysis.thdDb+8,"THD+N includes nonharmonic residual energy");
    for(int n=0;n<8192;++n) (*testInput)[size_t(n)]=float(0.1*(std::sin(tau*1000*n/48000)+std::sin(tau*1400*n/48000)));
    analysis=ToneBoyHarmonicAnalysis::analyse(*testInput,*testOutput,48000);
    check(analysis.status==ToneBoyHarmonicResult::Status::needsTone,"multitone is rejected as a harmonic test stimulus");
    const auto linear=[](double f,bool eq=false,float trim=0,bool pad=false){ return PWMToneBoyAudioProcessor::linearResponseDb(f,48000,eq,0,1,trim,pad); };
    check(std::abs((linear(1000,false,-6)-linear(1000))+6)<1e-6,"linear model includes OUTPUT gain");
    check(std::abs((linear(1000,false,0,true)-linear(1000))-20*std::log10(0.25))<1e-6,"linear model includes input PAD");
    check(linear(5)<linear(1000)-40 && linear(52)>linear(1000)+0.5,"linear model includes 18 Hz high-pass and bass contour");
    check(linear(15000,true)<linear(15000,false)-3,"linear model follows reconstruction switch");
    const auto measured52=measure(false,false,false,52);
    check(std::abs(measured52.gainDb-measured52.modelDb)<0.1 && std::abs(clean.gainDb-clean.modelDb)<0.1,"linear model agrees with measured small-signal DSP at 52 Hz and 1 kHz");
    const auto analog80=measure(false,false,false,80,true);
    
    check(std::abs(analog80.gainDb-analog80.modelDb)<0.1,"A-mode measured DSP agrees with displayed linear contour");
    check(PWMToneBoyAudioProcessor::linearResponseDb(65,48000,false,0,1,0,false,true,1)-linear(65)>0.9 && PWMToneBoyAudioProcessor::linearResponseDb(8000,48000,false,0,1,0,false,true,1)-linear(8000)<-0.8,"dynamic tonal load is applied only to A");
    check(neutral.parameter("analogMode")==0,"clean digital D is the default profile");
    check(PWMToneBoyAudioProcessor::processAnalogColour(0,1)==0,"A colour has no independent noise or DC source");
    check(analog80.thdDb > -90 && analog80.thdDb < -40,"A changes audio with gentle level-dependent harmonics");
    auto quietAnalog=std::make_unique<PWMToneBoyAudioProcessor>(); set(*quietAnalog,"analogMode",1); quietAnalog->prepareToPlay(48000,256);
    juce::AudioBuffer<float> quiet(2,256); quiet.clear(); for(int block=0;block<64;++block) quietAnalog->processBlock(quiet,midi);
    check(quiet.getMagnitude(0,256)==0,"A and KI together produce bit-exact silence from rest");
    set(*quietAnalog,"analogMode",0); quietAnalog->prepareToPlay(48000,256); quiet.clear(); for(int block=0;block<64;++block) quietAnalog->processBlock(quiet,midi);
    check(quiet.getMagnitude(0,256)==0 && PWMToneBoyAudioProcessor::processAnalogColour(0,1,1,true)==0,"digital D and KI also produce exact silence without noise injection");
    std::cout<<"Analog A: 80Hz gain="<<analog80.gainDb<<"dB, THD="<<analog80.thdDb<<"dB\n";

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
    std::function<DotMatrixLCDComponent*(juce::Component*)> findLcd=[&](juce::Component* component)->DotMatrixLCDComponent* { if(auto* screen=dynamic_cast<DotMatrixLCDComponent*>(component)) return screen; for(auto* child:component->getChildren()) if(auto* screen=findLcd(child)) return screen; return nullptr; };
    auto* screen=findLcd(editor.get()); check(screen!=nullptr,"editor contains its diagnostic LCD");
    if(screen)
    {
        for(int view=0;view<6;++view) { check(screen->getMode()==view,"LCD cycles through all six views"); screen->advanceView(); }
        check(screen->getMode()==0,"six LCD clicks return to spectrum");
        restored.prepareToPlay(48000,256); juce::AudioBuffer<float> signal(2,256);
        for(int block=0;block<1056;++block) { if(block==1024) juce::Thread::sleep(160); for(int n=0;n<256;++n) for(int c=0;c<2;++c) signal.setSample(c,n,float(0.2*std::sin(tau*1000.3*(block*256+n)/48000))); restored.processBlock(signal,midi); screen->refreshDiagnostics(); }
        check(screen->getHarmonicResult().status==ToneBoyHarmonicResult::Status::valid,"LCD analyses real final-output telemetry");
        check(screen->getHarmonicResult().thdnDb < -45,"settled real-output harmonic analysis rejects startup transients");
        editor->setSize(420,680);
        for(int view=1;view<=5;++view)
        {
            screen->advanceView();
            if(view>=4 && argc>1) { auto image=editor->createComponentSnapshot(editor->getLocalBounds()); auto out=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]); juce::FileOutputStream stream(out.getChildFile("UI-M"+juce::String(view)+".png")); stream.setPosition(0); stream.truncate(); juce::PNGImageFormat png; check(png.writeImageToStream(image,stream),"new analysis screen snapshot saved"); }
        }
    }
    if(screen)
    {
        juce::Thread::sleep(160); juce::AudioBuffer<float> music(2,256);
        for(int block=0;block<32;++block) { for(int n=0;n<256;++n) for(int c=0;c<2;++c) music.setSample(c,n,float(0.15*std::sin(tau*220*(block*256+n)/48000)+0.1*std::sin(tau*777*(block*256+n)/48000)+0.05*std::sin(tau*3290*(block*256+n)/48000))); restored.processBlock(music,midi); screen->refreshDiagnostics(); }
        check(screen->getHarmonicResult().status==ToneBoyHarmonicResult::Status::needsTone && screen->getSpectrumPeakDb()>-50,"M5 shows actual live music spectrum while withholding invalid THD");
        check(restored.analysisAnalogLoad.load()>0.1f && restored.analysisGainReduction.load()>0.1f,"live linear model is driven by actual DSP load and gain reduction");
        if(argc>1) { auto image=editor->createComponentSnapshot(editor->getLocalBounds()); auto out=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).getChildFile("UI-M5-Music.png"); juce::FileOutputStream stream(out); stream.setPosition(0); stream.truncate(); juce::PNGImageFormat png; check(png.writeImageToStream(image,stream),"music spectrum snapshot saved"); }
    }
    set(restored,"analogMode",1);
    juce::MemoryBlock analogState; restored.getStateInformation(analogState); set(restored,"analogMode",0); restored.setStateInformation(analogState.getData(),int(analogState.getSize()));
    check(restored.parameter("analogMode")==1,"A/D selector persists with saved state");
    if(screen && argc>1)
    {
        juce::AudioBuffer<float> analogSignal(2,256);
        for(int block=0;block<128;++block) { for(int n=0;n<256;++n) for(int c=0;c<2;++c) analogSignal.setSample(c,n,float(0.2*std::sin(tau*1000.3*(block*256+n)/48000))); restored.processBlock(analogSignal,midi); screen->refreshDiagnostics(); }
        for(int click=0;click<5;++click) screen->advanceView();
        auto image=editor->createComponentSnapshot(editor->getLocalBounds()); auto out=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]).getChildFile("UI-M4-A.png"); juce::FileOutputStream stream(out); stream.setPosition(0); stream.truncate(); juce::PNGImageFormat png; check(png.writeImageToStream(image,stream),"A-mode linear screen snapshot saved");
    }
    std::cout<<checks<<" checks, "<<failures<<" failures. Latency: "<<p.getLatencySamples()<<" samples.\n";
    return failures==0?EXIT_SUCCESS:EXIT_FAILURE;
}
