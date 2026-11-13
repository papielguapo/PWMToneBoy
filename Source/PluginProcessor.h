#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

struct ToneBoyScopeFrame { float before{}, after{}, dac{}, pwm{}, reduction{}, output{}, input{}; };

class PWMToneBoyAudioProcessor final : public juce::AudioProcessor
{
public:
    PWMToneBoyAudioProcessor();
    ~PWMToneBoyAudioProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PWM TONE BOY"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static float computeSCurveGainLin(float envelope, float threshold, float ratio) noexcept;
    static float processCraneTriodeEven(float x, float slow, float k2, float grid) noexcept;
    static float processPhoenixSapphireCeiling(float x, float& history) noexcept;
    static float processAnalogColour(float x,float memory,float load=0,bool dynamic=false) noexcept;
    struct AnalogProfile { float lowDb{},highDb{},lowHz{},highHz{}; };
    static AnalogProfile analogProfile(float modeMix,float load) noexcept;
    float parameter(const char* id) const { return apvts.getRawParameterValue(id)->load(); }
    int readScope(ToneBoyScopeFrame* destination, int maximum);
    double linearResponseDb(double frequency) const;
    static double linearResponseDb(double frequency,double rate,bool eq,float threshold,float ratio,float outputDb,bool pad,bool analog=false,float load=0,float modeMix=-1);
    std::atomic<double> analysisSampleRate{44100};
    std::atomic<unsigned> scopeOverruns{0};
    std::atomic<float> analysisAnalogLoad{0},analysisAnalogMix{0},analysisGainReduction{0};
    bool loadTestFile(const juce::File&);
    void setTestSource(int source);
    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inputDb{-100}, outputDb{-100}, reductionDb{0}, gateMeter{0}, kiMeter{0}, pdrMeter{0};
    std::atomic<int> editorWidth{420}, testSource{0};
    static constexpr std::array<float, 7> ratios{1,2,3,5,10,20,100};
    static constexpr std::array<float, 6> decays{100,200,400,800,1600,3200};
private:
    struct Biquad
    {
        // Double coefficient/state precision keeps the 18 Hz poles stable at 768 kHz.
        double b0{1}, b1{}, b2{}, a1{}, a2{}, z1{}, z2{};
        float tick(float x) noexcept { const auto y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y; return float(y); }
        void clear() { z1=z2=0; }
        void configure(double rate, float hz, float q, int type, float db=0);
    };
    struct Channel
    {
        Biquad scHpf, reconstruction, infra1, infra2, phoenix, analogLow, analogHigh;
        float fast{}, slow{}, rms{}, previousSc{}, held{}, ceiling{}, quietSeconds{};
        float gainReduction{}, releaseMemory{}, limiterGain{1.0f}, peakHold{};
        std::array<float, 512> dryDelay{};
        int dryPosition{};
    };
    void process(juce::AudioBuffer<float>&, bool hostBypass);
    void renderTestSource(juce::AudioBuffer<float>&);
    void processChunk(juce::AudioBuffer<float>&, int start, int count, bool hostBypass);
    std::array<Channel, 2> channels;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::AudioBuffer<float> scratch;
    std::vector<ToneBoyScopeFrame> scopeScratch;
    std::array<std::atomic<float>*,16> controls{};
    juce::SmoothedValue<float> analogSmooth;
    float analogLoad{},analogAttack{},analogRelease{};
    juce::SmoothedValue<float> outputSmooth;
    juce::SmoothedValue<float> thresholdSmooth, ratioSmooth, decaySmooth, clockSmooth, crushSmooth, padSmooth, bypassSmooth;
    juce::SmoothedValue<float> eqSmooth, hpfSmooth, linkSmooth, kiSmooth, pdaSmooth, weightSmooth, rawSmooth;
    double sampleRate{44100}, highRate{176400}, clockPhase{}, demoTime{};
    int maximumBlock{512}, latency{}, scopeDivider{};
    float fastAttack{}, snapAttack{}, fastRelease{}, rmsCoefficient{}, memoryAttack{}, memoryRelease{}, limiterRelease{};
    juce::AbstractFifo scopeFifo{8192};
    std::array<ToneBoyScopeFrame,8192> scopeStorage{};
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource fileTransport;
    std::unique_ptr<juce::AudioFormatReaderSource> fileReader;
    juce::AudioBuffer<float> fileScratch;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PWMToneBoyAudioProcessor)
};
