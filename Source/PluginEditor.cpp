#include "PluginEditor.h"

PWMToneBoyAudioProcessorEditor::PWMToneBoyAudioProcessorEditor(PWMToneBoyAudioProcessor& proc)
 : AudioProcessorEditor(proc),p(proc),panel(*this),lcd(proc)
{
    setLookAndFeel(&look); addAndMakeVisible(panel); panel.setSize(420,680);
    panel.addAndMakeVisible(lcd); lcd.setBounds(98,113,240,176);
    setupKnob(threshold,"threshold",false,{49,365,110,110},"PWM threshold; double-click restores -18 dB");
    setupKnob(ratio,"ratioIndex",true,{222,404,68,68},"B / Ratio: seven positions");
    setupKnob(decay,"decayIndex",true,{299,360,68,68},"A / Decay: six positions");
    setupKnob(clock,"clockRate",false,{161,505,48,48},"SELECT / PWM clock, 4 to 250 kHz");
    setupKnob(crush,"dacCrush",false,{214,505,48,48},"START / 4-bit DAC and even harmonics");
    setupButton(eq,"eqIn","toggle",{48,514,34,34},"Reconstruction filter: 11.2 kHz / 20 kHz");
    setupButton(hpf,"hpfIn","toggle",{94,514,34,34},"90 Hz Butterworth sidechain high-pass");
    setupButton(link,"linkAutoGain","lamp",{274,505,28,28},"85% stereo link + 15% independent");
    setupButton(bypass,"bypass","lamp",{309,505,28,28},"Latency-compensated bypass");
    setupButton(pad,"pad","lamp",{344,505,28,28},"Input pad x0.25 (-12.04 dB)");
    setupButton(ki,"kiMode","lamp",{274,553,28,28},"KI: gain-reduction-dependent even harmonics");
    setupButton(pda,"pdaMode","lamp",{309,553,28,28},"Program-dependent attack: 0.35 to 2.2 ms");
    setupButton(weight,"kWeightSc","lamp",{344,553,28,28},"H-SC: presence emphasis, 65% peak + 35% RMS");
    setupButton(phones,"phonesRawMode","jack",{53,591,50,44},"PHONES OUT / toggle PROSOUND 4B raw DAC");
    if(p.isStandalone())
    {
        addAndMakeVisible(testChoice); addAndMakeVisible(loadButton);
        testChoice.addItemList({"Audio input","Drums loop","808 Bass loop","Chiptune loop","File loop"},1);
        testChoice.setSelectedId(p.testSource.load()+1,juce::dontSendNotification);
        testChoice.onChange=[this]{ p.setTestSource(testChoice.getSelectedId()-1); };
        loadButton.onClick=[this]{ chooseFile(); };
    }
    setResizable(true,true); const double aspect=420.0/double(p.isStandalone()?712:680);
    getConstrainer()->setFixedAspectRatio(aspect);
    setResizeLimits(336,p.isStandalone()?570:544,840,p.isStandalone()?1424:1360);
    const int width=juce::jlimit(336,840,p.editorWidth.load()); setSize(width,juce::roundToInt(width/aspect));
    startTimerHz(30);
}
PWMToneBoyAudioProcessorEditor::~PWMToneBoyAudioProcessorEditor() { stopTimer(); chooser.reset(); setLookAndFeel(nullptr); }
void PWMToneBoyAudioProcessorEditor::setupKnob(juce::Slider& s,const char* id,bool magenta,juce::Rectangle<int> bounds,const juce::String& tip)
{
    panel.addAndMakeVisible(s); s.setBounds(bounds); s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    s.setRotaryParameters(juce::MathConstants<float>::pi*1.22f,juce::MathConstants<float>::pi*2.78f,true);
    s.getProperties().set("magenta",magenta); s.setTooltip(tip); s.setName(p.apvts.getParameter(id)->getName(80));
    s.setScrollWheelEnabled(false); s.setPopupDisplayEnabled(true,true,this); s.setMouseDragSensitivity(180);
    sliders.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts,id,s));
    const auto* param=p.apvts.getParameter(id); s.setDoubleClickReturnValue(true,param->convertFrom0to1(param->getDefaultValue()));
}
void PWMToneBoyAudioProcessorEditor::setupButton(juce::ToggleButton& b,const char* id,const char* kind,juce::Rectangle<int> bounds,const juce::String& tip)
{
    panel.addAndMakeVisible(b); b.setBounds(bounds); b.getProperties().set("kind",kind); b.setTooltip(tip); b.setName(p.apvts.getParameter(id)->getName(80));
    buttons.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,id,b));
}
void PWMToneBoyAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff292a30));
    if(p.isStandalone()) { g.setColour(juce::Colour(0xffd3d3dc)); g.setFont(ToneBoyLookAndFeel::font(10*float(getWidth())/420)); g.drawText("TEST",8,0,36,int(32*float(getWidth())/420),juce::Justification::centred); }
}
void PWMToneBoyAudioProcessorEditor::resized()
{
    const float scale=float(getWidth())/420; const int top=p.isStandalone()?juce::roundToInt(32*scale):0;
    panel.setTransform(juce::AffineTransform::scale(scale).translated(0,float(top)));
    panel.setTopLeftPosition(0,0);
    if(p.isStandalone()) { testChoice.setBounds(juce::roundToInt(48*scale),juce::roundToInt(4*scale),juce::roundToInt(236*scale),juce::roundToInt(24*scale)); loadButton.setBounds(juce::roundToInt(292*scale),juce::roundToInt(4*scale),juce::roundToInt(118*scale),juce::roundToInt(24*scale)); }
    p.editorWidth.store(getWidth());
}
void PWMToneBoyAudioProcessorEditor::paintPanel(juce::Graphics& g)
{
    using L=ToneBoyLookAndFeel;
    g.fillAll(juce::Colour(0xff34353c));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff62636b),0,0,juce::Colour(0xff292b30),420,680,false)); g.fillRoundedRectangle(4,4,412,672,5);
    for(int y=9;y<675;y+=3) { g.setColour(juce::Colours::white.withAlpha(0.018f)); g.drawHorizontalLine(y,7,413); }
    L::text(g,"500-SERIES KRAMER PIE / DMG-01",{45,9,330,15},10,juce::Colour(0xffd7d7d7));
    L::text(g,juce::String::fromUTF8("EVEN 2ND HARM • 24dB/OCT @ 18Hz"),{40,653,340,16},9,juce::Colour(0xffd7d7d7));
    for(float y:{28.0f,631.0f})
    {
        for(float x:{14.0f,390.0f}) { g.setColour(juce::Colour(0xff17181c)); g.fillRoundedRectangle(x,y,16,23,7); g.setColour(juce::Colour(0xff82838b)); g.drawRoundedRectangle(x,y,16,23,7,0.6f); }
        for(float x:{40.0f,380.0f}) { g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd5d6d9),x-4,y,juce::Colour(0xff55565f),x+4,y+9,false)); g.fillEllipse(x-4,y+1,8,8); g.setColour(juce::Colour(0xff33333c)); g.drawLine(x-2,y+3,x+2,y+7,1); }
    }
    juce::Path shell; shell.startNewSubPath(43,36); shell.lineTo(377,36); shell.quadraticTo(387,36,387,46); shell.lineTo(387,587); shell.quadraticTo(387,641,333,641); shell.lineTo(43,641); shell.quadraticTo(33,641,33,631); shell.lineTo(33,46); shell.quadraticTo(33,36,43,36); shell.closeSubPath();
    g.setColour(juce::Colours::black.withAlpha(0.45f)); g.fillPath(shell,juce::AffineTransform::translation(2,3));
    g.setColour(juce::Colour(0xffc8c6be)); g.fillPath(shell); g.setColour(juce::Colour(0xffecebe6)); g.strokePath(shell,juce::PathStrokeType(1));
    g.setColour(juce::Colour(0xff93938d)); g.drawHorizontalLine(63,34,386);
    g.setColour(juce::Colour(0xffaba9a3)); g.fillRoundedRectangle(49,42,102,16,8);
    L::text(g,juce::String::fromUTF8("◀ OFF•ON ▶"),{49,42,102,16},8,juce::Colour(0xff66666a)); L::text(g,"PROSOUND AUTO-GAIN",{204,42,163,16},8,juce::Colour(0xff64646a));
    juce::Path bezel; bezel.startNewSubPath(59,77); bezel.lineTo(361,77); bezel.quadraticTo(370,77,370,86); bezel.lineTo(370,291); bezel.quadraticTo(370,313,348,313); bezel.lineTo(59,313); bezel.quadraticTo(50,313,50,304); bezel.lineTo(50,86); bezel.quadraticTo(50,77,59,77); bezel.closeSubPath();
    g.setColour(juce::Colour(0xff6b6878)); g.fillPath(bezel); g.setColour(juce::Colour(0xff5a5667)); g.strokePath(bezel,juce::PathStrokeType(1));
    for(int i=0;i<2;++i) { g.setColour(i==0?juce::Colour(0xff7a2148):L::navy()); g.fillRect(63,89+i*4,58,2); g.fillRect(300,89+i*4,56,2); }
    L::text(g,"DOT MATRIX / STEREO SOUND",{124,85,172,14},7,juce::Colour(0xffd6d2d8));
    g.setColour(juce::Colour(0xff383e26)); g.fillRect(94,109,248,184);
    g.setColour(juce::Colour(p.parameter("bypass")>0.5f?0xff54323a:0xfffa3445)); g.fillEllipse(64,170,8,8);
    if(p.parameter("bypass")<0.5f) { g.setColour(juce::Colour(0xfffa7482)); g.fillEllipse(65,171,3,2); }
    L::text(g,"BATTERY",{54,182,34,9},5.5f,juce::Colour(0xffe3dee4));
    L::text(g,"CLICK LCD TO CHANGE VIEW",{110,296,215,10},6,juce::Colour(0xffd6d2d8));
    g.setColour(L::navy()); g.setFont(juce::Font(juce::FontOptions("Arial Black",22,juce::Font::bold))); g.drawText("PWM",53,326,72,27,juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions("Arial",24,juce::Font::bold|juce::Font::italic))); g.drawText("TONE BOY",126,326,190,27,juce::Justification::centredLeft);
    L::text(g,"PWM THRESHOLD",{44,355,120,12},9);
    g.setColour(juce::Colour(0xffa6a59f)); g.fillRoundedRectangle(66,475,78,20,3); L::text(g,juce::String(p.parameter("threshold"),1)+" dB",{66,475,78,20},12,juce::Colour(0xff30323c));
    L::text(g,juce::String::fromUTF8("A — DECAY"),{294,350,80,11},8); L::text(g,juce::String::fromUTF8("B — RATIO"),{216,391,80,11},8);
    const auto* rp=p.apvts.getParameter("ratioIndex"); const auto* dp=p.apvts.getParameter("decayIndex");
    L::text(g,rp->getCurrentValueAsText(),{223,470,68,15},10); L::text(g,dp->getCurrentValueAsText(),{299,428,68,15},10);
    L::text(g,"EQ IN",{43,497,44,12},8); L::text(g,"HPF SC",{89,497,44,12},8);
    L::text(g,"11.2kHz",{42,551,46,10},7); L::text(g,"90Hz",{89,551,44,10},7);
    g.saveState(); g.addTransform(juce::AffineTransform::rotation(-0.20944f,184,499)); L::text(g,"SELECT",{160,491,50,12},8); g.restoreState();
    g.saveState(); g.addTransform(juce::AffineTransform::rotation(-0.20944f,238,499)); L::text(g,"START",{214,491,48,12},8); g.restoreState();
    L::text(g,juce::String(p.parameter("clockRate"),1)+"k",{153,554,62,13},9); L::text(g,juce::String(p.parameter("dacCrush"),0)+"%",{213,554,52,13},9);
    const juce::String names[]{"LINK","BYPASS","PAD","KI","PDA","H-SC"};
    for(int i=0;i<6;++i) L::text(g,names[i],{float(270+(i%3)*35),float(i<3?493:541),36,12},7);
    L::text(g,"PHONES OUT",{43,580,70,12},8); L::text(g,"PROSOUND 4B",{43,629,72,8},6,p.parameter("phonesRawMode")>0.5f?juce::Colour(0xff227a45):juce::Colour(0xff777775));
    g.setColour(juce::Colour(0xffb6b4ad)); g.fillRoundedRectangle(138,592,111,29,5); g.setColour(juce::Colour(0xffdfddd6)); g.drawRoundedRectangle(138,592,111,29,5,0.7f);
    L::text(g,"AUTO-GAIN",{147,598,83,14},10,juce::Colour(0xff74746e)); g.setColour(juce::Colour(0xff388957)); g.fillEllipse(234,603,4,4);
    for(int i=0;i<6;++i) { juce::Path slot; slot.addRoundedRectangle(0,0,5,43,2.5f); const auto t=juce::AffineTransform::rotation(-0.488692f).translated(float(291+i*12),581.0f-float(i)*2); g.setColour(juce::Colour(0xff77767c)); g.fillPath(slot,t); g.setColour(juce::Colour(0xffeceae2).withAlpha(0.5f)); g.strokePath(slot,juce::PathStrokeType(0.6f),t); }
}
void PWMToneBoyAudioProcessorEditor::timerCallback() { panel.repaint(); }
void PWMToneBoyAudioProcessorEditor::chooseFile()
{
    chooser=std::make_unique<juce::FileChooser>("Load WAV or AIFF",juce::File{},"*.wav;*.aiff;*.aif");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<PWMToneBoyAudioProcessorEditor>(this)](const juce::FileChooser& fc)
    {
        if(safe==nullptr || fc.getResult()==juce::File{}) return;
        if(safe->p.loadTestFile(fc.getResult())) { safe->testChoice.setSelectedId(5,juce::dontSendNotification); safe->p.setTestSource(4); safe->loadButton.setTooltip(fc.getResult().getFullPathName()); }
        else juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Cannot read audio","Select a valid WAV or AIFF file.");
    });
}
