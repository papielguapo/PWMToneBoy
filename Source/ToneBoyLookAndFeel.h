#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class ToneBoyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    static juce::Colour navy() { return juce::Colour(0xff232a75); }
    static juce::Font font(float size,bool bold=true) { return juce::Font(juce::FontOptions("Arial",size,bold?juce::Font::bold:juce::Font::plain)); }
    static void text(juce::Graphics& g,const juce::String& s,juce::Rectangle<float> r,float size,juce::Colour colour=navy(),juce::Justification align=juce::Justification::centred)
    { g.setColour(colour); g.setFont(font(size)); g.drawText(s,r,align,false); }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider& slider) override
    {
        const auto area=juce::Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(4);
        const auto diameter=juce::jmin(area.getWidth(),area.getHeight());
        const auto cx=area.getCentreX(),cy=area.getCentreY(),radius=diameter*0.5f;
        g.setColour(juce::Colours::black.withAlpha(0.25f)); g.fillEllipse(cx-radius+1,cy-radius+4,diameter,diameter);
        juce::Path skirt;
        for(int i=0;i<96;++i) { const float a=float(i)*juce::MathConstants<float>::twoPi/96; const float r=radius*(i%4<2?1.0f:0.94f); const auto p=juce::Point<float>(cx+std::sin(a)*r,cy-std::cos(a)*r); if(i==0) skirt.startNewSubPath(p); else skirt.lineTo(p); }
        skirt.closeSubPath(); g.setColour(juce::Colour(0xff242329)); g.fillPath(skirt); g.setColour(juce::Colour(0xff56545b)); g.strokePath(skirt,juce::PathStrokeType(0.7f));
        const auto cap=juce::Rectangle<float>(cx-radius*0.76f,cy-radius*0.76f,diameter*0.76f,diameter*0.76f);
        const bool magenta=bool(slider.getProperties()["magenta"]);
        const auto c=magenta?juce::Colour(0xff6e1739):juce::Colour(0xffc6c8c9);
        g.setGradientFill(juce::ColourGradient(c.brighter(0.45f),cap.getX(),cap.getY(),c.darker(0.45f),cap.getRight(),cap.getBottom(),false));
        g.fillEllipse(cap); g.setColour(juce::Colours::black.withAlpha(0.55f)); g.drawEllipse(cap,1.2f);
        if(!magenta) { g.saveState(); juce::Path clip; clip.addEllipse(cap.reduced(1)); g.reduceClipRegion(clip); for(float yy=cap.getY();yy<cap.getBottom();yy+=1.7f) { g.setColour(juce::Colours::white.withAlpha(0.12f)); g.drawHorizontalLine(int(yy),cap.getX(),cap.getRight()); } g.restoreState(); }
        const auto a=start+pos*(end-start); const auto v=juce::Point<float>(std::sin(a),-std::cos(a));
        g.setColour(magenta?juce::Colours::white:navy()); g.drawLine({juce::Point<float>(cx,cy)+v*radius*0.32f,juce::Point<float>(cx,cy)+v*radius*0.68f},2.4f);
        g.setColour(juce::Colours::white.withAlpha(0.3f)); g.drawEllipse(cap.reduced(2),0.6f);
        for(int i=0;i<=10;++i) { const auto angle=start+float(i)/10*(end-start); const auto a1=juce::Point<float>(cx+std::sin(angle)*(radius+2),cy-std::cos(angle)*(radius+2)); const auto a2=juce::Point<float>(cx+std::sin(angle)*(radius+4),cy-std::cos(angle)*(radius+4)); g.setColour(navy().withAlpha(0.65f)); g.drawLine({a1,a2},0.7f); }
    }
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool hover,bool down) override
    {
        const bool on=b.getToggleState(); const auto kind=b.getProperties()["kind"].toString();
        const auto w=float(b.getWidth()),h=float(b.getHeight());
        if(kind=="toggle")
        {
            const auto cx=w*0.5f,cy=h*0.48f;
            g.setColour(juce::Colour(0xff777780)); g.fillEllipse(cx-9,cy-8,18,16);
            g.setColour(juce::Colour(0xff25252c)); g.fillEllipse(cx-5,cy-5,10,10);
            auto lever=juce::Rectangle<float>(cx-3,cy+(on?-13.0f:-1.0f),6,15);
            g.setGradientFill(juce::ColourGradient(juce::Colours::white,lever.getX(),0,juce::Colour(0xff777782),lever.getRight(),0,false)); g.fillRoundedRectangle(lever,2.5f); g.setColour(juce::Colour(0xff4e4e59)); g.drawRoundedRectangle(lever,2.5f,0.8f);
        }
        else if(kind=="jack")
        {
            auto r=juce::Rectangle<float>(w/2-15,5,30,30); g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff2f2ef),r.getTopLeft(),juce::Colour(0xff55545c),r.getBottomRight(),false)); g.fillEllipse(r);
            g.setColour(on?juce::Colour(0xff49e28a):juce::Colour(0xff89898d)); g.drawEllipse(r.reduced(3),2.2f);
            g.setColour(juce::Colour(0xff222229)); g.fillEllipse(r.reduced(7)); g.setColour(juce::Colours::black); g.fillEllipse(r.reduced(10));
        }
        else
        {
            auto r=juce::Rectangle<float>(w/2-9,4,18,18);
            if(on) { g.setColour(juce::Colours::white.withAlpha(0.16f)); g.fillEllipse(r.expanded(3)); }
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff2f2ef),r.getTopLeft(),juce::Colour(0xff53515d),r.getBottomRight(),false)); g.fillEllipse(r);
            g.setColour(juce::Colour(0xff55545f)); g.drawEllipse(r,0.8f);
            g.setColour(on?juce::Colour(0xffffffff):juce::Colour(0xffa1a1aa)); g.fillEllipse(r.reduced(3));
            g.setColour(juce::Colours::white.withAlpha(on?0.9f:0.3f)); g.fillEllipse(r.getX()+5,r.getY()+4,5,3);
        }
        if(hover || down) { g.setColour(navy().withAlpha(0.4f)); g.drawRoundedRectangle(b.getLocalBounds().toFloat().reduced(1),4,0.7f); }
    }
};
