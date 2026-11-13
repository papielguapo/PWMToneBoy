#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <complex>

namespace {
constexpr std::array<const char*,16> ids{"threshold","ratioIndex","decayIndex","clockRate","dacCrush","eqIn","hpfIn","linkAutoGain","bypass","pad","kiMode","pdaMode","kWeightSc","phonesRawMode","outputGain","analogMode"};
float db(float x) { return juce::Decibels::gainToDecibels(x, -100.0f); }
float coefficient(double rate, float ms) { return std::exp(-1.0f / (float(rate) * ms * 0.001f)); }
float mix(float a, float b, float t) { return a + (b-a)*t; }
}

juce::AudioProcessorValueTreeState::ParameterLayout PWMToneBoyAudioProcessor::createLayout()
{
    using ID=juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterFloat>(ID{"threshold",1},"PWM Threshold",juce::NormalisableRange<float>(-42,0,0.1f),-18.0f));
    p.add(std::make_unique<juce::AudioParameterChoice>(ID{"ratioIndex",1},"Ratio",juce::StringArray{"1.0:1","2.0:1","3.0:1","5.0:1","10.0:1","20.0:1","INF"},2));
    p.add(std::make_unique<juce::AudioParameterChoice>(ID{"decayIndex",1},"Decay",juce::StringArray{"100ms","200ms","400ms","800ms","1.6s","3.2s"},1));
    p.add(std::make_unique<juce::AudioParameterFloat>(ID{"clockRate",1},"PWM Clock (kHz)",juce::NormalisableRange<float>(4,250,0.1f,0.55f),120.0f));
    p.add(std::make_unique<juce::AudioParameterFloat>(ID{"dacCrush",1},"DAC Crush (%)",juce::NormalisableRange<float>(0,100,0.1f),0.0f));
    const std::array<const char*,9> names{"Reconstruction EQ","Sidechain HPF","Stereo Link","Bypass","Input Pad","KI Harmonics","Program Dependent Attack","Hybrid Sidechain","Phones ProSound 4B"};
    const std::array<bool,9> defaults{true,false,true,false,false,true,true,true,false};
    for (size_t i=0;i<names.size();++i)
        p.add(std::make_unique<juce::AudioParameterBool>(ID{ids[i+5],1},names[i],defaults[i]));
    p.add(std::make_unique<juce::AudioParameterFloat>(ID{"outputGain",1},"Output (dB)",juce::NormalisableRange<float>(-24,12,0.1f),0.0f));
    p.add(std::make_unique<juce::AudioParameterBool>(ID{"analogMode",1},"Analog A / Direct D",false));
    return p;
}

PWMToneBoyAudioProcessor::PWMToneBoyAudioProcessor()
 : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   apvts(*this,nullptr,"PWMToneBoyState",createLayout())
{
    for(size_t i=0;i<ids.size();++i) controls[i]=apvts.getRawParameterValue(ids[i]);
    formatManager.registerBasicFormats();
}
PWMToneBoyAudioProcessor::~PWMToneBoyAudioProcessor() { fileTransport.setSource(nullptr); }
bool PWMToneBoyAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return (l.getMainOutputChannelSet()==juce::AudioChannelSet::mono() || l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()) && l.getMainInputChannelSet()==l.getMainOutputChannelSet();
}
void PWMToneBoyAudioProcessor::Biquad::configure(double rate,float hz,float q,int type,float gain)
{
    const auto w=juce::MathConstants<double>::twoPi*juce::jlimit(1.0,double(rate*0.45),double(hz))/rate;
    const auto c=std::cos(w), s=std::sin(w), alpha=s/(2*q);
    double n0=1,n1=0,n2=0,d0=1,d1=-2*c,d2=1-alpha;
    if(type==0) { n0=(1+c)/2; n1=-(1+c); n2=n0; d0=1+alpha; }
    else if(type==1) { n0=(1-c)/2; n1=1-c; n2=n0; d0=1+alpha; }
    else { const auto a=std::pow(10.0,double(gain)/40); n0=1+alpha*a; n1=-2*c; n2=1-alpha*a; d0=1+alpha/a; d2=1-alpha/a; }
    b0=n0/d0; b1=n1/d0; b2=n2/d0; a1=d1/d0; a2=d2/d0;
}
void PWMToneBoyAudioProcessor::prepareToPlay(double rate,int block)
{
    sampleRate=rate>0?rate:44100; highRate=sampleRate*4; maximumBlock=juce::jmax(32,block);
    analysisSampleRate.store(sampleRate);
    scopeScratch.resize(size_t(maximumBlock));
    oversampling=std::make_unique<juce::dsp::Oversampling<float>>(2,2,juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);
    oversampling->initProcessing(size_t(maximumBlock));
    latency=juce::jlimit(0,511,juce::roundToInt(oversampling->getLatencyInSamples()));
    setLatencySamples(latency);
    scratch.setSize(2,maximumBlock); fileScratch.setSize(2,maximumBlock);
    fastAttack=coefficient(highRate,0.35f); snapAttack=coefficient(highRate,2.2f);
    fastRelease=coefficient(highRate,25); rmsCoefficient=coefficient(highRate,10);
    memoryAttack=coefficient(highRate,80); memoryRelease=coefficient(highRate,1400);
    limiterRelease=coefficient(highRate,60);
    for(auto& c:channels)
    {
        c=Channel{};
        c.scHpf.configure(highRate,90,0.70710678f,0);
        c.reconstruction.configure(highRate,11200,0.70710678f,1);
        c.infra1.configure(highRate,18,0.54119610f,0);
        c.infra2.configure(highRate,18,1.30656296f,0);
        c.phoenix.configure(highRate,52,0.78f,2,0.85f);
        c.analogLow.configure(highRate,80,0.65f,2,0.30f);
        c.analogHigh.configure(highRate,6000,0.50f,2,-0.25f);
    }
    auto init=[this](auto& s,float v){ s.reset(highRate,0.02); s.setCurrentAndTargetValue(v); };
    analogSmooth.reset(highRate,0.04); analogSmooth.setCurrentAndTargetValue(controls[15]->load());
    init(thresholdSmooth,parameter("threshold")); init(ratioSmooth,ratios[size_t(juce::jlimit(0,6,int(parameter("ratioIndex"))))]);
    init(decaySmooth,decays[size_t(juce::jlimit(0,5,int(parameter("decayIndex"))))]); init(clockSmooth,parameter("clockRate")); init(crushSmooth,0.0f);
    init(padSmooth,parameter("pad")>0.5f?0.25f:1); init(bypassSmooth,parameter("bypass"));
    init(eqSmooth,parameter("eqIn")); init(hpfSmooth,parameter("hpfIn")); init(linkSmooth,parameter("linkAutoGain"));
    init(kiSmooth,parameter("kiMode")); init(pdaSmooth,parameter("pdaMode")); init(weightSmooth,parameter("kWeightSc")); init(rawSmooth,0.0f); outputSmooth.reset(sampleRate,0.02); outputSmooth.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(controls[14]->load()));
    fileTransport.prepareToPlay(maximumBlock,sampleRate);
    reset();
}
void PWMToneBoyAudioProcessor::reset()
{
    if(oversampling) oversampling->reset();
    for(auto& c:channels) { c.fast=c.slow=c.rms=c.previousSc=c.held=c.ceiling=c.quietSeconds=0; c.gainReduction=c.releaseMemory=c.peakHold=0; c.limiterGain=1; c.dryDelay.fill(0); c.dryPosition=0; c.scHpf.clear(); c.reconstruction.clear(); c.infra1.clear(); c.infra2.clear(); c.phoenix.clear(); c.analogLow.clear(); c.analogHigh.clear(); }
    clockPhase=demoTime=0;
    // The SPSC diagnostic queue is intentionally not reset while an editor can consume it.
}
void PWMToneBoyAudioProcessor::releaseResources() { fileTransport.releaseResources(); }
float PWMToneBoyAudioProcessor::computeSCurveGainLin(float env,float threshold,float ratio) noexcept
{
    const float width=juce::jlimit(2.5f,10.5f,11-ratio*0.45f), delta=db(env)-threshold, slope=1-1/ratio;
    float reduction=0;
    if(delta>=width*0.5f) reduction=slope*delta;
    else if(delta> -width*0.5f) { const auto k=delta+width*0.5f; reduction=slope*k*k/(2*width); }
    return juce::Decibels::decibelsToGain(-reduction);
}
float PWMToneBoyAudioProcessor::processCraneTriodeEven(float x,float slow,float k2,float grid) noexcept
{
    juce::ignoreUnused(slow,grid);
    // Unity small-signal slope and an exactly linear KI-off path. The bounded even
    // term introduces gentle H2 without a half-wave logarithm or a DC generator.
    const auto square=x*x;
    return x+k2*square/(1.0f+0.25f*square);
}
float PWMToneBoyAudioProcessor::processPhoenixSapphireCeiling(float x,float& history) noexcept
{
    // C2-continuous safety knee: unity through +/-0.98. Normal audio never
    // passes through a permanently active saturator or waveform slew filter.
    const auto magnitude=std::abs(x);
    history=magnitude<=0.98f?x:std::copysign(0.98f+0.02f*std::tanh((magnitude-0.98f)/0.02f),x);
    return history;
}
float PWMToneBoyAudioProcessor::processAnalogColour(float x,float memory) noexcept
{
    // Original, noise-free class-A-inspired colour, not a recovered hardware circuit.
    // Unity small-signal slope; bounded smooth H2/H3 grow with signal level and
    // sustained gain-control activity. Zero input is exactly zero, without bias.
    const float square=x*x,drive=1.0f+0.2f*memory;
    return x+drive*(0.0035f*square/(1.0f+0.25f*square)-0.006f*x*square/(1.0f+square));
}
void PWMToneBoyAudioProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi) { midi.clear(); process(b,false); }
void PWMToneBoyAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer& midi) { midi.clear(); process(b,true); }
void PWMToneBoyAudioProcessor::process(juce::AudioBuffer<float>& b,bool hostBypass)
{
    juce::ScopedNoDenormals noDenormals;
    for(int c=getTotalNumInputChannels();c<b.getNumChannels();++c) b.clear(c,0,b.getNumSamples());
    if(!oversampling || b.getNumChannels()==0) return;
    if(isStandalone()) renderTestSource(b);
    thresholdSmooth.setTargetValue(controls[0]->load()); ratioSmooth.setTargetValue(ratios[size_t(juce::jlimit(0,6,int(controls[1]->load())))]);
    analogSmooth.setTargetValue(controls[15]->load());
    decaySmooth.setTargetValue(decays[size_t(juce::jlimit(0,5,int(controls[2]->load())))]); clockSmooth.setTargetValue(controls[3]->load()); crushSmooth.setTargetValue(0.0f);
    eqSmooth.setTargetValue(controls[5]->load()); hpfSmooth.setTargetValue(controls[6]->load()); linkSmooth.setTargetValue(controls[7]->load());
    bypassSmooth.setTargetValue(hostBypass?1:controls[8]->load()); padSmooth.setTargetValue(controls[9]->load()>0.5f?0.25f:1);
    kiSmooth.setTargetValue(controls[10]->load()); pdaSmooth.setTargetValue(controls[11]->load()); weightSmooth.setTargetValue(controls[12]->load()); rawSmooth.setTargetValue(0.0f); outputSmooth.setTargetValue(juce::Decibels::decibelsToGain(controls[14]->load()));
    for(int n=0;n<b.getNumSamples();n+=maximumBlock) processChunk(b,n,juce::jmin(maximumBlock,b.getNumSamples()-n),hostBypass);
}
void PWMToneBoyAudioProcessor::processChunk(juce::AudioBuffer<float>& b,int start,int count,bool)
{
    const int nc=juce::jmin(2,b.getNumChannels());
    for(int c=0;c<2;++c) scratch.copyFrom(c,0,b,juce::jmin(c,nc-1),start,count);
    juce::dsp::AudioBlock<float> all(scratch);
    auto base=all.getSubBlock(0,size_t(count));
    auto high=oversampling->processSamplesUp(base);
    float inPeak=0,outPeak=0,maxReduction=0,gate=0,ki=0,pdr=0;
    // Store a delayed dry path in the original buffer. It tracks the declared 4x latency.
    for(int n=0;n<count;++n) for(int c=0;c<nc;++c)
    {
        auto& ch=channels[size_t(c)]; const float input=b.getSample(c,start+n);
        ch.dryDelay[size_t(ch.dryPosition)]=input;
        b.setSample(c,start+n,ch.dryDelay[size_t((ch.dryPosition+512-latency)%512)]);
        ch.dryPosition=(ch.dryPosition+1)%512;
    }
    // Bypass weight is sampled at each host sample, with its ramp running at the internal rate.
    for(size_t n=0;n<high.getNumSamples();++n)
    {
        const auto threshold=thresholdSmooth.getNextValue(), ratio=ratioSmooth.getNextValue();
        const auto decayMs=decaySmooth.getNextValue();
        const auto clock=clockSmooth.getNextValue()*1000, crush=crushSmooth.getNextValue(), pad=padSmooth.getNextValue();
        const auto eq=eqSmooth.getNextValue(), hpf=hpfSmooth.getNextValue(), link=linkSmooth.getNextValue();
        const auto kiOn=kiSmooth.getNextValue(), pda=pdaSmooth.getNextValue(), weight=weightSmooth.getNextValue(), raw=rawSmooth.getNextValue();
        const auto makeup=juce::Decibels::decibelsToGain((-threshold*(1-1/ratio))*0.54f);
        const float bypass=bypassSmooth.getNextValue();
        const float analog=analogSmooth.getNextValue();
        clockPhase+=juce::jmin(double(clock)/highRate,1.0);
        const bool latch=clockPhase>=1; if(latch) clockPhase-=1;
        float gain[2]{}, input[2]{}, gates[2]{};
        for(int c=0;c<2;++c)
        {
            auto& ch=channels[size_t(c)];
            float x=high.getSample(c,int(n)); if(!std::isfinite(x)) x=0; x=juce::jlimit(-16.0f,16.0f,x)*pad;
            input[c]=x; inPeak=juce::jmax(inPeak,std::abs(x));
            const auto filtered=ch.scHpf.tick(x); auto sc=mix(x,filtered,hpf);
            const auto emphasized=sc+0.65f*(sc-ch.previousSc); ch.previousSc=sc; sc=mix(sc,emphasized,weight);
            ch.rms=rmsCoefficient*ch.rms+(1-rmsCoefficient)*sc*sc;
            const auto det=mix(std::abs(sc),0.65f*std::abs(sc)+0.35f*std::sqrt(juce::jmax(0.0f,ch.rms)),weight);
            // A-MOD-inspired coupling: the peak circuit SHORTENS the nominal
            // attack near overload; it does not slow down a steep transient.
            const auto predictedPeak=std::abs(x)*makeup*juce::Decibels::decibelsToGain(-ch.gainReduction);
            const auto peakDemand=juce::jlimit(0.0f,1.0f,(predictedPeak-0.75f)/0.25f);
            const auto attack=mix(snapAttack,fastAttack,peakDemand*pda);
            ch.fast=det+(ch.fast-det)*(det>ch.fast?attack:fastRelease);
            const auto memoryTarget=juce::jlimit(0.0f,1.0f,ch.gainReduction/18.0f);
            ch.releaseMemory=memoryTarget+(ch.releaseMemory-memoryTarget)*(memoryTarget>ch.releaseMemory?memoryAttack:memoryRelease);
            const auto release=coefficient(highRate,decayMs*(1.0f+2.5f*ch.releaseMemory));
            ch.slow=det+(ch.slow-det)*(det>ch.slow?attack:release);
            const auto crest=juce::jlimit(0.0f,1.0f,(ch.fast-ch.slow)/(ch.fast+1.0e-5f));
            const auto slowWeight=juce::jlimit(0.25f,0.9f,0.45f+0.4f*ch.releaseMemory-0.2f*crest);
            const auto envelope=mix(ch.fast,ch.slow,slowWeight);
            const auto targetGr=-db(computeSCurveGainLin(envelope,threshold,ratio));
            // Gain-domain ballistics reduce control ripple; stored program energy
            // lengthens recovery after sustained compression, independently of DECAY.
            const auto grRelease=coefficient(highRate,juce::jmax(35.0f,decayMs*0.30f)*(1.0f+3.0f*ch.releaseMemory));
            ch.gainReduction=targetGr+(ch.gainReduction-targetGr)*(targetGr>ch.gainReduction?attack:grRelease);
            gain[c]=juce::Decibels::decibelsToGain(-ch.gainReduction);
            gates[c]=juce::jlimit(0.0f,1.0f,ch.fast*ch.fast*196);
            pdr=ch.releaseMemory;
        }
        if(nc==1) gain[1]=gain[0];
        const auto mean=(gain[0]+gain[1])*0.5f;
        ToneBoyScopeFrame frame;
        float postMakeup[2]{};
        for(int c=0;c<2;++c)
        {
            auto& ch=channels[size_t(c)]; const auto g=mix(gain[c],mean,link*0.85f);
            const auto k2=kiOn*(0.008f+0.012f*(1-g))*gates[c];
            auto x=processCraneTriodeEven(input[c]*g,ch.slow,k2,1+crush*1.8f);
            if(latch) ch.held=x;
            x=mix(x,ch.held,mix(crush,1.0f,raw));
            // Asymmetric mid-tread DAC: exactly 16/12 codes including exact zero.
            const auto quantize=[](float v,int codes){ const float half=float(codes/2); return juce::jlimit(-half,half-1,std::round(v*half))/half; };
            const auto quantized=mix(quantize(x,16),quantize(x,12),raw);
            const auto dac=mix(x,quantized,mix(crush,1.0f,raw)*gates[c]);
            if((n&31u)==0u) ch.reconstruction.configure(highRate,mix(20000.0f,11200.0f,eq),0.70710678f,1);
            const auto reconstructed=ch.reconstruction.tick(dac);
            const auto analogContour=ch.analogHigh.tick(ch.analogLow.tick(reconstructed));
            const auto preInfra=mix(reconstructed,processAnalogColour(analogContour,ch.releaseMemory),analog);
            // All asymmetric colour precedes the 18 Hz blocker; no DC generated
            // by the optional A stage is left downstream of the infrasound filters.
            const auto infra=ch.infra2.tick(ch.infra1.tick(preInfra));
            const auto contour=ch.phoenix.tick(infra);
            postMakeup[c]=contour*makeup;
            if(c==0)
            {
                const float pulse=clockPhase < double(juce::jlimit(0.05f,0.95f,0.5f+input[c]*0.45f)) ? 1.0f : -1.0f;
                frame={preInfra,infra,dac,pulse,-db(g),0}; gate=gates[c]; ki=k2;
            }
        }
        const auto maximumPeak=juce::jmax(std::abs(postMakeup[0]),std::abs(postMakeup[nc-1]));
        for(int c=0;c<2;++c)
        {
            auto& ch=channels[size_t(c)];
            const auto level=mix(std::abs(postMakeup[c]),maximumPeak,link);
            const auto requiredGain=level>0.95f?0.95f/level:1.0f;
            if(requiredGain<ch.limiterGain) { ch.limiterGain=requiredGain; ch.peakHold=float(highRate*0.0005); }
            else if(ch.peakHold>0) ch.peakHold-=1;
            else ch.limiterGain=requiredGain+(ch.limiterGain-requiredGain)*limiterRelease;
        }
        const auto commonLimiter=juce::jmin(channels[0].limiterGain,channels[size_t(nc-1)].limiterGain);
        for(int c=0;c<2;++c)
        {
            auto& ch=channels[size_t(c)];
            const auto limiting=mix(ch.limiterGain,commonLimiter,link);
            auto out=processPhoenixSapphireCeiling(postMakeup[c]*limiting,ch.ceiling);
            if(std::abs(input[c])<1e-12f && std::abs(out)<1e-12f) out=0;
            high.setSample(c,int(n),out);
            maxReduction=juce::jmax(maxReduction,-db(mix(gain[c],mean,link*0.85f)*limiting));
            if(c==0) { frame.output=out; frame.reduction-=db(limiting); }
        }
        // One ramp value per base sample, stored in scratch only after upsampling has consumed it.
        if((n&3u)==3u) scratch.setSample(0,int(n/4),bypass);
        if((n&3u)==3u) scopeScratch[n/4]=frame;
    }
    // Preserve ramp values: scratch is also the downsampling destination.
    // Channel 1 of fileScratch is spare outside the test-source generation step.
    for(int n=0;n<count;++n) fileScratch.setSample(1,n,scratch.getSample(0,n));
    oversampling->processSamplesDown(base);
    outPeak=0;
    for(int n=0;n<count;++n)
    {
      const float outputGain=outputSmooth.getNextValue();
      for(int c=0;c<nc;++c)
      {
        if(c==0) scopeScratch[size_t(n)].input=b.getSample(c,start+n);
        const float output=mix(scratch.getSample(c,n)*outputGain,b.getSample(c,start+n),fileScratch.getSample(1,n));
        b.setSample(c,start+n,output); outPeak=juce::jmax(outPeak,std::abs(output));
    }
      scopeScratch[size_t(n)].output=b.getSample(0,start+n);
    }
    int s1,z1,s2,z2; scopeFifo.prepareToWrite(count,s1,z1,s2,z2);
    for(int n=0;n<z1;++n) scopeStorage[size_t(s1+n)]=scopeScratch[size_t(n)];
    for(int n=0;n<z2;++n) scopeStorage[size_t(s2+n)]=scopeScratch[size_t(z1+n)];
    scopeFifo.finishedWrite(z1+z2);
    if(z1+z2<count) scopeOverruns.fetch_add(1,std::memory_order_relaxed);
    inputDb.store(db(inPeak)); outputDb.store(db(outPeak)); reductionDb.store(maxReduction); gateMeter.store(gate); kiMeter.store(ki); pdrMeter.store(pdr);
}
int PWMToneBoyAudioProcessor::readScope(ToneBoyScopeFrame* destination,int maximum)
{
    int s1,z1,s2,z2; scopeFifo.prepareToRead(maximum,s1,z1,s2,z2);
    for(int i=0;i<z1;++i) destination[i]=scopeStorage[size_t(s1+i)];
    for(int i=0;i<z2;++i) destination[z1+i]=scopeStorage[size_t(s2+i)];
    scopeFifo.finishedRead(z1+z2); return z1+z2;
}
double PWMToneBoyAudioProcessor::linearResponseDb(double frequency,double rate,bool eq,float threshold,float ratio,float trim,bool pad,bool analog)
{
    rate=juce::jmax(8000.0,rate);
    frequency=juce::jlimit(1.0,rate*0.49,frequency);
    const double internalRate=rate*4;
    std::array<Biquad,6> filters;
    filters[0].configure(internalRate,18,0.54119610f,0);
    filters[1].configure(internalRate,18,1.30656296f,0);
    filters[2].configure(internalRate,52,0.78f,2,0.85f);
    filters[3].configure(internalRate,eq?11200.0f:20000.0f,0.70710678f,1);
    if(analog) { filters[4].configure(internalRate,80,0.65f,2,0.30f); filters[5].configure(internalRate,6000,0.50f,2,-0.25f); }
    const auto z=std::polar(1.0,-juce::MathConstants<double>::twoPi*frequency/internalRate);
    double magnitude=1;
    for(const auto& filter:filters) magnitude*=std::abs((filter.b0+filter.b1*z+filter.b2*z*z)/(1.0+filter.a1*z+filter.a2*z*z));
    return 20*std::log10(juce::jmax(1e-12,magnitude))+double(-threshold*(1-1/ratio)*0.54f+trim)+(pad?20*std::log10(0.25):0);
}
double PWMToneBoyAudioProcessor::linearResponseDb(double frequency) const
{
    if(parameter("bypass")>0.5f) return 0;
    const auto ratio=ratios[size_t(juce::jlimit(0,6,int(parameter("ratioIndex"))))];
    return linearResponseDb(frequency,analysisSampleRate.load(),parameter("eqIn")>0.5f,parameter("threshold"),ratio,parameter("outputGain"),parameter("pad")>0.5f,parameter("analogMode")>0.5f);
}
juce::AudioProcessorParameter* PWMToneBoyAudioProcessor::getBypassParameter() const { return apvts.getParameter("bypass"); }
void PWMToneBoyAudioProcessor::getStateInformation(juce::MemoryBlock& b)
{
    auto state=apvts.copyState(); state.setProperty("editorWidth",editorWidth.load(),nullptr);
    if(auto xml=state.createXml()) copyXmlToBinary(*xml,b);
}
void PWMToneBoyAudioProcessor::setStateInformation(const void* data,int size)
{
    if(auto xml=getXmlFromBinary(data,size); xml && xml->hasTagName(apvts.state.getType()))
    {
        auto state=juce::ValueTree::fromXml(*xml);
        editorWidth.store(juce::jlimit(336,840,int(state.getProperty("editorWidth",420))));
        apvts.replaceState(state);
    }
}
bool PWMToneBoyAudioProcessor::loadTestFile(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if(!reader) return false;
    testSource.store(0); fileTransport.stop(); fileTransport.setSource(nullptr);
    fileReader=std::make_unique<juce::AudioFormatReaderSource>(reader.release(),true); fileReader->setLooping(true);
    fileTransport.setSource(fileReader.get(),0,nullptr,fileReader->getAudioFormatReader()->sampleRate);
    return true;
}
void PWMToneBoyAudioProcessor::setTestSource(int source)
{
    testSource.store(juce::jlimit(0,4,source));
    if(source==4 && fileReader) fileTransport.start(); else fileTransport.stop();
}
void PWMToneBoyAudioProcessor::renderTestSource(juce::AudioBuffer<float>& b)
{
    const int source=testSource.load(); if(source==0) return;
    if(source==4)
    {
        for(int pos=0;pos<b.getNumSamples();pos+=maximumBlock)
        {
            const int count=juce::jmin(maximumBlock,b.getNumSamples()-pos); fileScratch.clear();
            fileTransport.getNextAudioBlock({&fileScratch,0,count});
            for(int c=0;c<b.getNumChannels();++c) b.copyFrom(c,pos,fileScratch,juce::jmin(c,1),0,count);
        }
        return;
    }
    for(int n=0;n<b.getNumSamples();++n)
    {
        const double beat=std::fmod(demoTime,0.5), t=beat;
        float x=0;
        if(source==1) { const auto kick=std::sin(juce::MathConstants<double>::twoPi*(48*t+6*(1-std::exp(-t*45)))); const auto hat=std::sin(demoTime*62137)*std::sin(demoTime*48763); x=float(kick*std::exp(-t*17)*0.65+hat*std::exp(-std::fmod(demoTime,0.125)*130)*0.17); }
        if(source==2) x=float(0.65*std::sin(juce::MathConstants<double>::twoPi*(41.2*t+1.2*(1-std::exp(-t*30))))*std::exp(-t*5));
        if(source==3) { constexpr int notes[]{60,64,67,72,67,64,62,55}; const int step=int(std::fmod(demoTime*8,8)); const double f=juce::MidiMessage::getMidiNoteInHertz(notes[step]); x=std::fmod(demoTime*f,1.0)<0.25?0.22f:-0.22f; x*=float(std::exp(-std::fmod(demoTime,0.125)*10)); }
        for(int c=0;c<b.getNumChannels();++c) b.setSample(c,n,x);
        demoTime+=1/sampleRate;
    }
}
juce::AudioProcessorEditor* PWMToneBoyAudioProcessor::createEditor() { return new PWMToneBoyAudioProcessorEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PWMToneBoyAudioProcessor(); }
