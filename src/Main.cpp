#include "Engine.h"

class MainComponent final : public juce::Component, private juce::Timer {
    Engine engine;
    juce::Label title, status, metrics, hint;
    juce::ComboBox source, genre, key, midi, channel, section;
    juce::TextButton start{"Start session"}, stop{"Stop"}, tap{"Tap tempo"}, reset{"Re-sync"};
    juce::TextButton load{"Load drum WAV"}, save{"Save metrics"};
    juce::ToggleButton click{"Following click"}, bass{"AI bassist (rule-based)"};
    juce::ToggleButton guitar{"AI guitar"}, piano{"AI piano"};
    juce::Slider bassLevel,guitarLevel,pianoLevel;
    juce::Label bassLevelLabel,guitarLevelLabel,pianoLevelLabel;
    juce::Slider volume, density, variation, gate, timing;
    juce::Label volumeLabel, densityLabel, variationLabel, gateLabel, timingLabel;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> selector;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<juce::MidiDeviceInfo> midiDevices;
    double lastTap=0;
    std::array<double,5> taps{}; int tapCount=0;
    juce::String localError;
    uint64_t lastCallbackCount=0;
    double lastCallbackChangeMs=juce::Time::getMillisecondCounterHiRes();
public:
    MainComponent() {
        title.setText("ENSEMBLECUE  /  LISTEN. FOLLOW. PLAY.",juce::dontSendNotification);
        title.setFont(juce::Font(juce::FontOptions(24,juce::Font::bold)));
        addAndMakeVisible(title); addAndMakeVisible(status); addAndMakeVisible(metrics); addAndMakeVisible(hint);
        status.setFont(juce::Font(juce::FontOptions(24))); metrics.setFont(juce::Font(juce::FontOptions(14)));
        hint.setText("Mic is analysis-only. Start with PA low. Auto key chooses a key; drums do not reveal harmony.",juce::dontSendNotification);
        for(auto* c : {&source,&genre,&key,&midi,&channel,&section}) addAndMakeVisible(*c);
        section.addItemList({"Section: Auto","Section: Groove","Section: Build","Section: Breakdown"},1);
        section.setSelectedId(1);section.onChange=[this]{engine.sectionMode=section.getSelectedId()-1;};
        source.addItemList({"Live microphone","Synthetic drums (demo)","Loaded drum WAV"},1); source.setSelectedId(1); source.setItemEnabled(3,false);
        source.onChange=[this]{engine.stopSession(); engine.source=source.getSelectedId()-1; ++engine.resetRequest;};
        genre.addItemList({"Rock / straight","Funk / syncopated","Shuffle"},1); genre.setSelectedId(1);
        genre.onChange=[this]{engine.genre=genre.getSelectedId()-1;};
        key.addItem("Auto key (minor)",1); key.addItemList({"C minor","C# minor","D minor","Eb minor","E minor","F minor","F# minor","G minor","Ab minor","A minor","Bb minor","B minor"},2);
        key.setSelectedId(1); key.onChange=[this]{engine.key=key.getSelectedId()-2;};
        channel.addItemList({"Input 1","Input 2 (enable 2 channels below)"},1); channel.setSelectedId(1);
        channel.onChange=[this]{engine.inputChannel=channel.getSelectedId()-1;};
        midi.addItem("MIDI output: none (internal synth)",1);
        midiDevices=juce::MidiOutput::getAvailableDevices();
        for(int i=0;i<midiDevices.size();++i) midi.addItem(midiDevices[i].name,i+2);
        midi.setSelectedId(1); midi.onChange=[this]{int i=midi.getSelectedId()-2; localError=engine.selectMidi(i>=0?midiDevices[i].identifier:juce::String{});};
        for(auto* b : {&start,&stop,&tap,&reset,&load,&save}) addAndMakeVisible(*b);
        start.onClick=[this]{localError.clear(); engine.startSession();}; stop.onClick=[this]{engine.stopSession();};
        reset.onClick=[this]{++engine.resetRequest;};
        tap.onClick=[this]{
            double now=juce::Time::getMillisecondCounterHiRes()*.001;
            double dt=now-lastTap;
            if(dt>=1./3 && dt<=1) {
                taps[static_cast<size_t>(tapCount++%5)]=dt;
                auto copy=taps; int n=std::min(tapCount,5); std::sort(copy.begin(),copy.begin()+n);
                engine.tapBpm=60/copy[static_cast<size_t>(n/2)];
            } else tapCount=0;
            lastTap=now;
        };
        load.onClick=[this]{
            chooser=std::make_unique<juce::FileChooser>("Choose isolated drum audio",juce::File{},"*.wav;*.aiff;*.aif");
            chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const auto& fc){
                auto f=fc.getResult(); if(f.existsAsFile()) {localError=engine.loadFile(f); if(localError.isEmpty()) {source.setItemEnabled(3,true); source.setSelectedId(3);}}
            });
        };
        save.onClick=[this]{
            chooser=std::make_unique<juce::FileChooser>("Save callback measurements",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("ensemblecue-metrics.csv"),"*.csv");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[this](const auto& fc){if(fc.getResult()!=juce::File{}) engine.exportMetrics(fc.getResult());});
        };
        addAndMakeVisible(click); addAndMakeVisible(bass); click.setToggleState(true,juce::dontSendNotification);
        click.onClick=[this]{engine.click=click.getToggleState();}; bass.onClick=[this]{engine.bass=bass.getToggleState();};
        addAndMakeVisible(guitar);addAndMakeVisible(piano);
        guitar.onClick=[this]{engine.guitar=guitar.getToggleState();};
        piano.onClick=[this]{engine.piano=piano.getToggleState();};
        auto slider=[this](juce::Slider& s,juce::Label& label,const char* text,double value) {
            addAndMakeVisible(s); addAndMakeVisible(label); label.setText(text,juce::dontSendNotification);
            s.setRange(0,1,.01); s.setValue(value); s.setTextBoxStyle(juce::Slider::TextBoxRight,false,55,24);
        };
        slider(volume,volumeLabel,"Output volume",.3); slider(density,densityLabel,"Band density",.55);
        slider(bassLevel,bassLevelLabel,"Bass level",1);
        slider(guitarLevel,guitarLevelLabel,"Guitar level",.7);
        slider(pianoLevel,pianoLevelLabel,"Piano level",.7);
        bassLevel.onValueChange=[this]{engine.bassVolume=static_cast<float>(bassLevel.getValue());};
        guitarLevel.onValueChange=[this]{engine.guitarVolume=static_cast<float>(guitarLevel.getValue());};
        pianoLevel.onValueChange=[this]{engine.pianoVolume=static_cast<float>(pianoLevel.getValue());};
        slider(variation,variationLabel,"Variation",.35); slider(gate,gateLabel,"Onset gate (dB)",.3);
        gate.setRange(-60,-12,1); gate.setValue(-44);
        slider(timing,timingLabel,"Timing trim (ms)",0); timing.setRange(-100,150,1); timing.setValue(0);
        timing.onValueChange=[this]{engine.timingOffsetMs=static_cast<float>(timing.getValue());};
        volume.onValueChange=[this]{engine.volume=static_cast<float>(volume.getValue());};
        density.onValueChange=[this]{engine.density=static_cast<float>(density.getValue());};
        variation.onValueChange=[this]{engine.variation=static_cast<float>(variation.getValue());};
        gate.onValueChange=[this]{engine.gate=juce::Decibels::decibelsToGain(static_cast<float>(gate.getValue()));};
        selector=std::make_unique<juce::AudioDeviceSelectorComponent>(engine.devices,0,2,0,2,false,false,true,false);
        addAndMakeVisible(*selector); setSize(900,940); startTimerHz(20);
    }
    ~MainComponent() override { stopTimer(); chooser.reset(); selector.reset(); }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff101b24));
        g.setColour(juce::Colour(0xff253640)); g.fillRoundedRectangle(22,116,856,16,5);
        const float level=static_cast<float>(engine.level.load());
        float meter=std::clamp((juce::Decibels::gainToDecibels(level,-60.f)+60)/60,0.f,1.f);
        g.setColour(engine.inputPeak.load()>.98?juce::Colours::orange:juce::Colour(0xff70ddaa));
        g.fillRoundedRectangle(22,116,856*meter,16,5);
        for(int i=0;i<4;++i) {
            bool on=engine.active && (static_cast<int>(engine.beat.load())%4)==i;
            g.setColour(on?juce::Colour(0xff70ddaa):juce::Colour(0xff253640));
            g.fillEllipse(740.f+i*32,72,22,22);
        }
    }
    void resized() override {
        title.setBounds(22,16,856,36); status.setBounds(22,62,705,42); metrics.setBounds(22,140,856,54);
        source.setBounds(22,205,270,30); channel.setBounds(306,205,265,30); load.setBounds(588,205,140,30); save.setBounds(740,205,138,30);
        start.setBounds(22,248,175,36); stop.setBounds(210,248,115,36); tap.setBounds(340,248,135,36); reset.setBounds(490,248,110,36);
        click.setBounds(615,248,160,36); bass.setBounds(22,298,260,30);
        genre.setBounds(300,298,185,30); key.setBounds(500,298,170,30); midi.setBounds(22,342,400,30);
        volumeLabel.setBounds(445,342,135,30); volume.setBounds(580,342,298,30);
        densityLabel.setBounds(22,386,110,30); density.setBounds(135,386,280,30);
        variationLabel.setBounds(445,386,110,30); variation.setBounds(560,386,318,30);
        gateLabel.setBounds(22,427,140,30); gate.setBounds(165,427,250,30);
        timingLabel.setBounds(445,427,140,30); timing.setBounds(590,427,288,30);
        bassLevelLabel.setBounds(22,462,120,28);bassLevel.setBounds(145,462,270,28);
        section.setBounds(500,462,378,28);
        guitar.setBounds(22,494,130,28);guitarLevelLabel.setBounds(160,494,100,28);guitarLevel.setBounds(270,494,260,28);
        piano.setBounds(22,526,130,28);pianoLevelLabel.setBounds(160,526,100,28);pianoLevel.setBounds(270,526,260,28);
        selector->setBounds(22,564,856,315); hint.setBounds(22,894,856,30);
    }
    void timerCallback() override {
        auto callbacks=engine.callbackCount.load();
        double now=juce::Time::getMillisecondCounterHiRes();
        if(callbacks!=lastCallbackCount) {lastCallbackCount=callbacks; lastCallbackChangeMs=now;}
        bool stalled=engine.playing && now-lastCallbackChangeMs>1000;
        juce::String state=!engine.playing?"STOPPED":(!engine.active?"LISTENING":(engine.locked?"FOLLOWING":"ACQUIRING"));
        if(stalled) state="DEVICE STALLED";
        status.setText(juce::String(engine.bpm.load(),1)+" BPM    "+state+"    "+juce::String(juce::roundToInt(engine.confidence.load()*100))+"%",juce::dontSendNotification);
        auto* dev=engine.devices.getCurrentAudioDevice();
        auto err=engine.getError(); if(err.isEmpty()) err=localError;
        if(stalled) err="No audio callbacks. Close other audio apps, then reselect or reset the device.";
        if(!dev && err.isEmpty()) err="No audio device. Select a device below.";
        if(engine.fileEnded && err.isEmpty()) err="File ended. Press Start to replay.";
        const char* sections[]={"Groove","Build","Breakdown"};
        const char* roots[]={"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
        int root=engine.currentRoot.load(), sectionId=engine.currentSection.load();
        juce::String harmony=root>=0?juce::String("Root ")+roots[root%12]:"Harmony acquiring";
        if(sectionId>=0 && sectionId<3) harmony+=" / "+juce::String(sections[sectionId]);
        metrics.setText(juce::String(engine.sampleRate.load(),0)+" Hz / "+juce::String(engine.bufferSize.load())+" frames  |  CPU callback "+juce::String(engine.devices.getCpuUsage()*100,1)+"%  |  Xruns "+juce::String(dev?dev->getXRunCount():-1)
            +"  |  Queue drops "+juce::String(engine.droppedFeatures.load())+"\n"+(err.isEmpty()?harmony+(engine.fill?" / Fill: leaving space":" / Settings apply up to 2 bars ahead"):err),juce::dontSendNotification);
        repaint();
    }
};
class EnsembleCueApp final : public juce::JUCEApplication {
    class Window final : public juce::DocumentWindow {
    public:
        Window() : DocumentWindow("EnsembleCue",juce::Colour(0xff101b24),allButtons) {
            setUsingNativeTitleBar(true); setContentOwned(new MainComponent(),true); centreWithSize(getWidth(),getHeight()); setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
public:
    const juce::String getApplicationName() override { return "EnsembleCue"; }
    const juce::String getApplicationVersion() override { return "0.2.0"; }
    void initialise(const juce::String&) override {window=std::make_unique<Window>();}
    void shutdown() override {window.reset();}
};
START_JUCE_APPLICATION(EnsembleCueApp)
