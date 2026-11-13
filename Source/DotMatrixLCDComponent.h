#pragma once
#include "PluginProcessor.h"
#include "ToneBoyLookAndFeel.h"

class DotMatrixLCDComponent final : public juce::Component, private juce::Timer
{
public:
    explicit DotMatrixLCDComponent(PWMToneBoyAudioProcessor& processor):p(processor),fft(10)
    {
        setTitle("LCD diagnostics"); setDescription("Click to change spectrum, triode, compressor or PWM view");
        setMouseCursor(juce::MouseCursor::PointingHandCursor); startTimerHz(60);
    }
    void mouseDown(const juce::MouseEvent&) override { mode=(mode+1)%4; repaint(); }
    int getMode() const { return mode; }
    void paint(juce::Graphics& g) override
    {
        g.saveState(); g.addTransform(juce::AffineTransform::scale(float(getWidth())/240,float(getHeight())/176));
        const auto ink=juce::Colour(0xff27381f),light=juce::Colour(0xffd3e680);
        g.fillAll(juce::Colour(0xff9cae66));
        for(int x=2;x<240;x+=3) for(int y=2;y<176;y+=3) { g.setColour(ink.withAlpha(0.035f)); g.fillRect(x,y,2,2); }
        const juce::String titles[]{"M0: FFT SPECTRUM","M1: KI TRIODE+18Hz","M2: STC-8M KNEE+PDR","M3: 4BIT DAC+PWM"};
        label(g,titles[mode],5,3,230,10,8.5f); g.setColour(ink.withAlpha(0.7f)); g.drawHorizontalLine(16,5,235);
        if(mode==0)
        {
            for(int i=0;i<48;++i)
            {
                const float height=juce::jlimit(0.0f,77.0f,(spectrum[size_t(i)]+72)*77/72);
                g.setColour(ink); for(int yy=0;yy<int(height);yy+=4) g.fillRect(5+i*4.8f,100.0f-float(yy),3.0f,2.5f);
                g.setColour(light); g.fillRect(5+i*4.8f,100.0f-juce::jlimit(0.0f,77.0f,(peaks[size_t(i)]+72)*77/72),3.0f,2.0f);
            }
            label(g,"30       100       1k       5k      20k",5,106,230,9,7);
        }
        if(mode==1)
        {
            trace(g,0,23,76,ink,true); trace(g,1,23,76,light,false);
            label(g,"KI:"+juce::String(p.kiMeter.load()*100,0)+"%  G:"+juce::String(p.gateMeter.load()*100,0)+"%  24dB/18Hz",5,105,230,10,7.5f);
        }
        if(mode==2)
        {
            g.setColour(ink.withAlpha(0.3f)); g.drawRect(6,23,96,74); g.drawLine(6,97,102,23,0.5f);
            juce::Path curve;
            const auto r=PWMToneBoyAudioProcessor::ratios[size_t(juce::jlimit(0,6,int(p.parameter("ratioIndex"))))];
            for(int i=0;i<96;++i) { const auto in=-48+float(i)*0.5f; const auto gain=PWMToneBoyAudioProcessor::computeSCurveGainLin(juce::Decibels::decibelsToGain(in),p.parameter("threshold"),r); const auto out=in+juce::Decibels::gainToDecibels(gain); const auto y=97-(out+48)*74/48; if(i==0) curve.startNewSubPath(6,y); else curve.lineTo(float(6+i),y); }
            g.setColour(light); g.strokePath(curve,juce::PathStrokeType(1.5f));
            juce::Path history; for(int i=0;i<120;++i) { const auto y=27+juce::jlimit(0.0f,65.0f,grHistory[size_t((grWrite+i)%120)]*2); if(i==0) history.startNewSubPath(112,y); else history.lineTo(float(112+i),y); }
            g.setColour(ink); g.strokePath(history,juce::PathStrokeType(1)); label(g,"GR HISTORY",116,90,110,9,7);
            label(g,"PDA:"+juce::String(p.parameter("pdaMode")>0.5f?"ON":"OFF")+" H-SC:"+(p.parameter("kWeightSc")>0.5f?"ON":"OFF")+" PDR:"+juce::String(p.pdrMeter.load()*100,0)+"%",5,105,230,10,7.5f);
        }
        if(mode==3) { trace(g,2,23,43,ink,false); trace(g,3,74,25,light,false); label(g,juce::String(p.parameter("clockRate"),1)+"kHz / "+(p.parameter("phonesRawMode")>0.5f?"12 RAW":"16 DAC")+" / "+juce::String(p.parameter("dacCrush"),0)+"%",5,105,230,10,7.5f); }
        g.setColour(ink); g.drawHorizontalLine(119,5,235);
        label(g,"0   10  20  30  40  50  60  70  80  90 B",5,121,230,9,7);
        meter(g,"IN",p.inputDb.load(),133,false); meter(g,"GR",p.reductionDb.load(),146,true); meter(g,"OUT",p.outputDb.load(),159,false);
        g.restoreState();
    }
private:
    void label(juce::Graphics& g,const juce::String& s,int x,int y,int w,int h,float size)
    { g.setColour(juce::Colour(0xff27381f)); g.setFont(juce::Font(juce::FontOptions("Consolas",size,juce::Font::bold))); g.drawText(s,x,y,w,h,juce::Justification::centredLeft); }
    void meter(juce::Graphics& g,const char* name,float value,int y,bool gr)
    {
        label(g,name,5,y,25,10,8); const auto amount=gr?juce::jlimit(0.0f,1.0f,value/30):juce::jlimit(0.0f,1.0f,(value+60)/60);
        for(int i=0;i<39;++i) { g.setColour(juce::Colour(0xff27381f).withAlpha(float(i)<amount*39?0.94f:0.12f)); g.fillRect(32+i*4,y+2,3,7); }
        label(g,value<=-99?"-INF":juce::String(value,1),191,y,47,10,8);
    }
    void trace(juce::Graphics& g,int which,float top,float height,juce::Colour colour,bool dotted)
    {
        g.setColour(juce::Colour(0xff27381f).withAlpha(0.2f)); g.drawLine(5,top+height/2,235,top+height/2,0.5f);
        juce::Path path;
        for(int i=0;i<230;++i)
        {
            const auto& f=wave[size_t((waveWrite+i*2)%1024)];
            const float value=which==0?f.before:which==1?f.after:which==2?f.dac:f.pwm*0.7f;
            const float y=top+height*0.5f-juce::jlimit(-1.0f,1.0f,value)*height*0.45f;
            if(i==0 || (dotted && i%5==0)) path.startNewSubPath(float(i+5),y); else if(!dotted || i%5<3) path.lineTo(float(i+5),y);
        }
        g.setColour(colour); g.strokePath(path,juce::PathStrokeType(1.25f));
    }
    void timerCallback() override
    {
        std::array<ToneBoyScopeFrame,4096> frames{};
        const int count=p.readScope(frames.data(),int(frames.size()));
        for(int i=0;i<count;++i)
        {
            wave[size_t(waveWrite)]=frames[size_t(i)]; waveWrite=(waveWrite+1)%1024;
            fftInput[size_t(fftWrite++)]=frames[size_t(i)].output;
            if(fftWrite==1024)
            {
                fftWrite=0; fftData.fill(0);
                for(int j=0;j<1024;++j) fftData[size_t(j)]=fftInput[size_t(j)]*(0.5f-0.5f*std::cos(juce::MathConstants<float>::twoPi*float(j)/1023));
                fft.performFrequencyOnlyForwardTransform(fftData.data());
                const auto rate=p.getSampleRate()>0?p.getSampleRate():44100;
                for(int bar=0;bar<48;++bar) { const auto hz=30*std::pow(20000.0/30,double(bar)/47); const int bin=juce::jlimit(1,511,int(hz*1024/rate)); spectrum[size_t(bar)]=juce::Decibels::gainToDecibels(fftData[size_t(bin)]/256,-90.0f); }
            }
        }
        if(count==0) for(auto& v:spectrum) v=juce::jmax(-90.0f,v-1.5f);
        for(size_t i=0;i<48;++i) peaks[i]=juce::jmax(spectrum[i],peaks[i]-0.45f);
        grHistory[size_t(grWrite)]=p.reductionDb.load(); grWrite=(grWrite+1)%120;
        repaint();
    }
    PWMToneBoyAudioProcessor& p;
    juce::dsp::FFT fft;
    std::array<float,2048> fftData{};
    std::array<float,1024> fftInput{};
    std::array<float,48> spectrum=[] { std::array<float,48> a{}; a.fill(-90); return a; }(),peaks=spectrum;
    std::array<ToneBoyScopeFrame,1024> wave{};
    std::array<float,120> grHistory{};
    int mode{},waveWrite{},fftWrite{},grWrite{};
};
