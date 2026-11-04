#include "Engine.h"
#include <chrono>

Engine::Engine() : juce::Thread("Rhythm and phrase worker") {
    error=devices.initialise(1,2,nullptr,true);
    devices.addAudioCallback(this);
}
Engine::~Engine() { playing=false; devices.removeAudioCallback(this); stopThread(-1); devices.closeAudioDevice(); }
void Engine::sendAllNotesOff() {
    if(midi) for(int channel=1;channel<=3;++channel) midi->sendMessageNow(juce::MidiMessage::allNotesOff(channel));
}
void Engine::queueMidi(MidiEvent event) noexcept {if(!midiEvents.push(event)) ++droppedMidi;}
void Engine::releaseMidi(int channel) noexcept {
    for(auto& note:pendingMidi) if(note.pitch>=0 && note.channel==channel) {
        queueMidi({note.pitch,0,audioEpoch,channel});note.pitch=-1;
    }
}
void Engine::startSession() {
    playing=false;
    devices.removeAudioCallback(this); stopThread(-1);
    filePosition=0; fileEnded=false;
    devices.addAudioCallback(this);
    if(auto* dev=devices.getCurrentAudioDevice()) initialXruns=dev->getXRunCount();
    playing=true;
}
void Engine::stopSession() { playing=false; ++resetRequest; }
juce::String Engine::loadFile(const juce::File& path) {
    stopSession();
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(path));
    if(!reader) return "Cannot read WAV/AIFF file.";
    if(reader->lengthInSamples<=0 || reader->lengthInSamples>reader->sampleRate*600) return "Use a file between 0 and 600 seconds.";
    devices.removeAudioCallback(this); stopThread(-1);
    file.setSize(1,static_cast<int>(reader->lengthInSamples));
    const bool ok=reader->read(&file,0,file.getNumSamples(),0,true,false);
    fileRate=reader->sampleRate; filePosition=0; source=2; fileEnded=false;
    devices.addAudioCallback(this);
    return ok?juce::String{}:"Audio read failed.";
}
juce::String Engine::selectMidi(const juce::String& id) {
    stopSession(); devices.removeAudioCallback(this); stopThread(-1);
    sendAllNotesOff();
    midi=id.isEmpty()?nullptr:juce::MidiOutput::openDevice(id);
    devices.addAudioCallback(this);
    return !id.isEmpty() && !midi?"MIDI device could not be opened.":juce::String{};
}
void Engine::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    stopThread(-1);
    sr=device->getCurrentSampleRate(); sampleRate=sr;
    bufferSize=device->getCurrentBufferSizeSamples();
    inputLatency=device->getInputLatencyInSamples(); outputLatency=device->getOutputLatencyInSamples();
    features.reset(); states.reset(); phrases.reset(); midiEvents.reset();
    extractor.prepare(sr); tracker.reset(); generator=band::Arranger{}; clock.reset(); performance.prepare(sr);
    samples=0; latest={}; audioEpoch=0; pendingMidi.fill({}); gain=0;
    demoPhase=0; demoAge=hatAge=100; demoBeat=0;
    bpm=110; confidence=0; locked=false; active=false;
    { juce::ScopedLock lock(errorMutex); error.clear(); }
    startThread();
}
void Engine::audioDeviceStopped() {
    stopThread(-1); active=false; locked=false;
    sendAllNotesOff();
}
void Engine::audioDeviceError(const juce::String& message) {
    playing=false; juce::ScopedLock lock(errorMutex); error=message;
}
void Engine::run() {
    uint32_t request=resetRequest.load(); bool wasPlaying=false;
    int64_t planned=-1; uint32_t plannedEpoch=0;
    uint32_t previousMidiDrops=droppedMidi.load();
    while(!threadShouldExit()) {
        band::Feature f; bool worked=false;
        while(features.pop(f)) {
            worked=true;
            bool play=playing.load(); auto reset=resetRequest.load();
            if(reset!=request || play!=wasPlaying) {
                tracker.reset(f.time,bpm.load()); planned=-1; request=reset;
                sendAllNotesOff();
            }
            wasPlaying=play;
            double tap=tapBpm.exchange(0);
            if(tap>=60 && tap<=180) tracker.tap(f.time,tap);
            tracker.setGate(gate.load());
            auto state=play?tracker.process(f):tracker.current();
            if(!play) { state.time=f.time; state.active=false; state.locked=false; }
            states.push(state); // Drop newest telemetry if full; audio consumes the last complete value.
            bpm=state.bpm; beat=state.beat; confidence=state.confidence; active=state.active;
            locked=state.locked; fill=state.fill; pattern=state.pattern; onsetDensity=state.density;
            if(state.active && state.locked) {
                if(plannedEpoch!=state.epoch) { planned=-1; plannedEpoch=state.epoch; }
                auto bar=static_cast<int64_t>(state.beat/4);
                if(planned<bar-1) planned=bar-1;
                band::Settings settings{key.load(),static_cast<band::Genre>(genre.load()),density.load(),variation.load()};
                while(planned<bar+2) {
                    auto phrase=generator.generate(planned+1,state,settings,sectionMode.load());
                    if(!phrases.push(phrase)) break;
                    ++planned;
                }
            }
        }
        MidiEvent event;
        auto midiDrops=droppedMidi.load();
        if(midiDrops!=previousMidiDrops) {sendAllNotesOff();previousMidiDrops=midiDrops;}
        while(midiEvents.pop(event)) if(midi) {
            if(event.velocity==0) midi->sendMessageNow(juce::MidiMessage::noteOff(event.channel,event.pitch));
            else if(event.epoch==tracker.current().epoch && playing.load())
                midi->sendMessageNow(juce::MidiMessage::noteOn(event.channel,event.pitch,event.velocity));
        }
        if(!worked) wait(1);
    }
}
void Engine::audioDeviceIOCallbackWithContext(const float* const* in,int ni,float* const* out,int no,int n,const juce::AudioIODeviceCallbackContext&) {
    const auto begin=std::chrono::steady_clock::now();
    band::State update;
    while(states.pop(update)) latest=update;
    if(latest.epoch!=audioEpoch) {
        for(int channel=1;channel<=3;++channel) releaseMidi(channel);
        audioEpoch=latest.epoch; performance.reset(audioEpoch);
    }
    band::Arrangement phrase;
    while(phrases.pop(phrase)) performance.submit(phrase);
    auto projected=latest;
    projected.beat+=((inputLatency.load()+outputLatency.load())/sr+timingOffsetMs.load()*.001)*latest.bpm/60;
    clock.follow(projected,samples/sr);
    const bool audible=playing.load() && clock.active();
    const std::array<bool,3> enabled{bass.load(),guitar.load() && !latest.fill,piano.load() && !latest.fill};
    const std::array<float,3> volumes{bassVolume.load(),guitarVolume.load(),pianoVolume.load()};
    const bool useClick=click.load(); const int src=source.load();
    for(int track=0;track<3;++track) if(!audible || !enabled[static_cast<size_t>(track)]) releaseMidi(track+1);
    if(!audible) performance.release();
    double sum=0; float peak=0;
    for(int i=0;i<n;++i,++samples) {
        const double t=samples/sr;
        float input=0;
        if(src==0 && ni>0) { int ch=std::clamp(inputChannel.load(),0,ni-1); if(in[ch]) input=in[ch][i]; }
        else if(src==1 && playing.load()) {
            double tempo=110+8*std::sin(t*.08);
            double prev=demoPhase; demoPhase+=tempo/(60*sr);
            if(static_cast<int>(demoPhase)>static_cast<int>(prev) || samples==0) { demoAge=0; ++demoBeat; }
            if(static_cast<int>(demoPhase*2)>static_cast<int>(prev*2)) hatAge=0;
            input=static_cast<float>(.7*std::exp(-demoAge*65)*std::sin(2*band::pi*(demoBeat%2?70:280)*demoAge)
                +.1*std::exp(-hatAge*160)*std::sin(2*band::pi*7000*hatAge));
            demoAge+=1/sr; hatAge+=1/sr;
        } else if(src==2 && playing.load() && file.getNumSamples()>1) {
            int index=static_cast<int>(filePosition);
            if(index+1<file.getNumSamples()) {
                float frac=static_cast<float>(filePosition-index);
                input=file.getSample(0,index)*(1-frac)+file.getSample(0,index+1)*frac;
                filePosition+=fileRate/sr;
            } else fileEnded=true;
        }
        sum+=input*input; peak=std::max(peak,std::abs(input));
        band::Feature f;
        if(extractor.process(input,t,f) && !features.push(f)) ++droppedFeatures;
        if(audible) {
            double b=clock.advance(sr);
            currentRoot=performance.rootAt(b);currentSection=performance.sectionAt(b);
            for(auto& note:pendingMidi) if(note.pitch>=0 && b>=note.offBeat) {
                queueMidi({note.pitch,0,audioEpoch,note.channel});note.pitch=-1;
            }
            performance.tick(b,latest.bpm,useClick,enabled,[&](int track,int pitch,float vel,double duration){
                // Repeated pitches receive their note-off before the new attack.
                for(auto& note:pendingMidi) if(note.pitch==pitch && note.channel==track+1) {
                    queueMidi({pitch,0,audioEpoch,track+1});note.pitch=-1;
                }
                auto slot=std::find_if(pendingMidi.begin(),pendingMidi.end(),[](const auto& note){return note.pitch<0;});
                if(slot==pendingMidi.end()) {
                    slot=std::min_element(pendingMidi.begin(),pendingMidi.end(),[](const auto& a,const auto& c){return a.offBeat<c.offBeat;});
                    queueMidi({slot->pitch,0,audioEpoch,slot->channel});
                }
                queueMidi({pitch,vel,audioEpoch,track+1});
                *slot={pitch,track+1,b+duration*latest.bpm/60};
            });
        }
        gain+=(volume.load()-gain)*.001;
        float value=static_cast<float>(gain)*performance.process(volumes);
        // File/demo monitoring only; the live microphone is never routed back to PA.
        if(src!=0 && playing.load()) value+=input*.22f*static_cast<float>(gain);
        value=std::clamp(value,-.95f,.95f);
        for(int ch=0;ch<no;++ch) if(out[ch]) out[ch][i]=value;
    }
    level=std::sqrt(sum/std::max(1,n)); inputPeak=peak;
    double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
    if(us>callbackMaxUs.load()) callbackMaxUs=us;
    callbackTotalUs.fetch_add(us); ++callbackCount;
    if(us>n/sr*1e6) ++overruns;
}
void Engine::exportMetrics(const juce::File& path) {
    auto* dev=devices.getCurrentAudioDevice();
    juce::String text="metric,value\n";
    auto row=[&](juce::String name,juce::String value){text+=name+","+value+"\n";};
    row("sample_rate",juce::String(sampleRate.load())); row("buffer_frames",juce::String(bufferSize.load()));
    row("input_latency_frames_reported",juce::String(inputLatency.load())); row("output_latency_frames_reported",juce::String(outputLatency.load()));
    row("callback_max_us",juce::String(callbackMaxUs.load()));
    row("callback_mean_us",juce::String(callbackTotalUs.load()/std::max(uint64_t{1},callbackCount.load())));
    row("callback_deadline_exceedances",juce::String(overruns.load()));
    row("driver_xruns",juce::String(dev?dev->getXRunCount():-1));
    row("driver_xruns_initial",juce::String(initialXruns.load()));
    row("driver_xruns_session_delta",juce::String(dev && initialXruns>=0 && dev->getXRunCount()>=0?dev->getXRunCount()-initialXruns.load():-1));
    row("feature_queue_drops",juce::String(droppedFeatures.load())); row("midi_queue_drops",juce::String(droppedMidi.load()));
    row("loopback_latency_ms","not_measured"); path.replaceWithText(text);
}
