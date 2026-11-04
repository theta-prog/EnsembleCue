#include "Core.h"

namespace band {
void FeatureExtractor::prepare(double sampleRate) {
    *this=FeatureExtractor{}; sr=sampleRate; hop=std::max(1,static_cast<int>(sr*.005));
    aLow=1-std::exp(-2*pi*180/sr); aBody=1-std::exp(-2*pi*1800/sr);
}
bool FeatureExtractor::process(float x, double t, Feature& f) noexcept {
    if(!std::isfinite(x)) x=0;
    low+=static_cast<float>(aLow)*(x-low); body+=static_cast<float>(aBody)*(x-body);
    energy+=x*x; lowEnergy+=low*low; bodyEnergy+=body*body;
    peak=std::max(peak,std::abs(x));
    if(++count<hop) return false;
    f={t,static_cast<float>(std::sqrt(energy/count)),static_cast<float>(std::sqrt(lowEnergy/count)),
        static_cast<float>(std::sqrt(bodyEnergy/count)),peak};
    count=0; energy=lowEnergy=bodyEnergy=0; peak=0; return true;
}
void Tracker::reset(double t, double bpm) {
    const auto e=state.epoch+1;
    *this=Tracker{}; state.time=t; state.bpm=std::clamp(bpm,60.,180.); state.epoch=e;
}
void Tracker::tap(double t, double bpm) {
    const bool wasActive=state.active;
    reset(t,bpm); state.locked=true; state.confidence=.65f;
    state.active=wasActive; state.beat=0; lastStrong=t; firstOnset=t; lastOnset=t;
    // Tap preserves existing activity, but does not invent microphone activity.
}
void Tracker::onset(double t,float w) {
    if(count==128) { std::move(onsets.begin()+1,onsets.end(),onsets.begin()); --count; }
    onsets[count++]={t,w};
    if(!state.active) {
        state.active=true; firstOnset=t; state.beat=0;
        state.epoch++; state.pattern=0;
    }
    lastOnset=t; peakWeight=std::max(w,peakWeight*.985f);
    if(w>peakWeight*.32f) {
        lastStrong=t;
        if(state.locked) {
            const double nearest=std::round(state.beat);
            const double error=nearest-state.beat;
            if(std::abs(error)<.23) {
                state.beat+=error*(state.fill?.06:.65)*std::max(.25f,state.confidence);
                if(!state.fill) state.bpm=std::clamp(state.bpm+error*4*state.confidence,60.,180.);
                state.confidence=std::min(1.f,state.confidence+.018f);
            }
        }
    }
    int slot=(static_cast<int>(std::lround(state.beat*4))%16+16)%16;
    state.pattern |= static_cast<uint16_t>(1u<<slot);
}
void Tracker::estimate(double t) {
    if(count<4 || t-firstOnset<1.4) return;
    // Weighted interval comb. Strong drum-body transients outweigh quiet hats.
    double best=-1, bestBpm=state.bpm;
    std::array<double,121> scores{};
    for(int b=60;b<=180;++b) {
        double score=0; const double period=60./b;
        for(int i=0;i<count;++i) for(int j=i+1;j<count;++j) {
            const double dt=onsets[j].time-onsets[i].time;
            if(dt>period*4.12) break;
            const int k=static_cast<int>(std::lround(dt/period));
            if(k<1 || k>4) continue;
            const double err=(dt-k*period)/(.025+.006*k);
            const double age=t-onsets[j].time;
            score+=std::sqrt(onsets[i].weight*onsets[j].weight)*std::exp(-.5*err*err-age/2.5)/k;
        }
        // Continuity prior is deliberately small: sustained changes must win.
        if(state.locked) score*=1+.18*std::exp(-.5*std::pow((b-state.bpm)/8,2));
        scores[static_cast<size_t>(b-60)]=score;
        if(score>best) { best=score; bestBpm=b; }
    }
    if(best<=0) return;
    double competitor=0;
    for(int b=60;b<=180;++b) if(std::abs(b-bestBpm)>12) competitor=std::max(competitor,scores[static_cast<size_t>(b-60)]);
    float confidence=static_cast<float>(std::clamp((best-competitor)/best*2.5,.15,1.));
    if(!state.locked) {
        state.bpm=bestBpm; state.locked=true;
        // Establish phase at the strongest recent onset, not a random hi-hat.
        int strong=0;
        for(int i=1;i<count;++i) if(onsets[i].weight>onsets[strong].weight) strong=i;
        state.beat=std::round((onsets[strong].time-firstOnset)*bestBpm/60)+(t-onsets[strong].time)*bestBpm/60;
    } else {
        double delta=bestBpm-state.bpm;
        bool octave=std::abs(bestBpm/state.bpm-2)<.12 || std::abs(bestBpm/state.bpm-.5)<.06;
        if(std::abs(delta)>15) {
            if(std::abs(bestBpm-pendingTempo)<5) ++pendingCount; else pendingCount=1;
            pendingTempo=bestBpm;
            if(pendingCount<(octave?12:5)) delta=0;
        } else pendingCount=0;
        if(!state.fill) state.bpm+=std::clamp(delta*(.12+.35*confidence),-4.,4.);
    }
    state.confidence=.7f*state.confidence+.3f*confidence;
}
State Tracker::process(const Feature& f) {
    const double dt=std::max(0.,f.time-state.time);
    if(state.active) {
        auto oldBar=static_cast<int64_t>(state.beat/4);
        state.beat+=dt*state.bpm/60;
        if(static_cast<int64_t>(state.beat/4)!=oldBar) state.pattern=0;
    }
    state.time=f.time; state.level=f.rms;
    // Remove history on silence so a restart can acquire a different tempo.
    while(count>0 && f.time-onsets[0].time>6) {
        std::move(onsets.begin()+1,onsets.begin()+count,onsets.begin()); --count;
    }
    float strength=f.low*.65f+f.body*.35f;
    float novelty=std::max(0.f,strength-previous);
    average=.985f*average+.015f*strength;
    if(f.rms>gate && novelty>std::max(gate*.45f,average*.65f) && f.time-lastOnset>.075)
        onset(f.time,novelty);
    previous=strength;
    int recent=0;
    for(int i=0;i<count;++i) if(f.time-onsets[i].time<1) ++recent;
    state.density=static_cast<float>(recent);
    state.fill=state.locked && recent>state.bpm/60*3.3;
    if(f.time-lastOnset>std::max(1.25,1.6*60/state.bpm)) {
        if(state.active) { state.active=false; state.locked=false; state.confidence=0; count=0; state.epoch++; }
    }
    if(f.time-lastStrong>.8) state.confidence*=.998f;
    if(f.time-lastEstimate>.25 && state.active) { estimate(f.time); lastEstimate=f.time; }
    return state;
}
float Generator::random() { seed^=seed<<13; seed^=seed>>17; seed^=seed<<5; return (seed&0xffffff)/16777216.f; }
Generator::Generator() {
    for(int i=0;i<16;++i) motif[i]=(i%4==0)?0:((i%4==2)?7:12);
    autoKey=static_cast<int>(random()*12);
}
Phrase Generator::generate(int64_t bar,const State& state,Settings settings) {
    Phrase p; p.bar=bar; p.epoch=state.epoch;
    int key=settings.key<0?autoKey:settings.key%12;
    if(bar>0 && bar%16==0 && random()<settings.variation) std::swap(progression[1],progression[2]);
    constexpr int scale[]={0,2,3,5,7,8,10};
    p.root=(key+scale[progression[static_cast<size_t>((bar/2)%4)]])%12;
    const int degree=progression[static_cast<size_t>((bar/2)%4)];
    p.third=(scale[(degree+2)%7]-scale[degree]+12)%12;
    p.fifth=(scale[(degree+4)%7]-scale[degree]+12)%12;
    float density=std::clamp(settings.density*.7f+state.density/12*.3f,.1f,1.f);
    int steps=settings.genre==Genre::Funk?16:8;
    if(bar>0 && bar%4==0 && random()<settings.variation) {
        const int slot=1+static_cast<int>(random()*15);
        if(slot%4!=0) motif[static_cast<size_t>(slot)]=random()<.5f?7:12;
    }
    for(int i=0;i<steps;++i) {
        bool anchor=i% (steps/4)==0;
        bool drumAccent=(state.pattern & (1u<<(i*16/steps)))!=0;
        float probability=std::clamp(density+(drumAccent?.18f:0.f),0.f,1.f);
        if(!anchor && random()>probability*(state.fill?.28f:1.f)) continue;
        double offset=i*4./steps;
        if(settings.genre==Genre::Shuffle && i%2) offset+=1./6;
        int degree=motif[static_cast<size_t>(i)];
        if(i>0 && random()<settings.variation*.25f) degree=(random()<.6f)?7:10;
        if(anchor && i==0) degree=0;
        // Anticipate the next chord by a semitone only at a phrase ending.
        int pitch=36+p.root+degree;
        if(pitch>55) pitch-=12;
        p.notes[static_cast<size_t>(p.count++)]={bar*4.+offset, settings.genre==Genre::Funk?.18:.38,
            pitch,std::clamp(.35f+state.level*2+(anchor?.15f:0),.25f,.9f)};
    }
    return p;
}
void BeatClock::follow(const State& s,double now) {
    if(!s.active || !s.locked || now-s.time>.25) { running=false; return; }
    double target=s.beat+(now-s.time)*s.bpm/60;
    if(!running || epoch!=s.epoch) { beat=target; epoch=s.epoch; }
    else {
        double error=target-beat;
        // Never rewind: speed nudging avoids duplicate notes at phase correction.
        bpm=std::clamp(s.bpm+error*120,45.,210.);
    }
    if(!running) bpm=s.bpm;
    running=true;
}
void Synth::note(int pitch,float vel,double seconds) noexcept {
    freq=440*std::pow(2.,(pitch-69)/12.); velocity=vel; age=0; length=seconds; releasing=false;
}
float Synth::process() noexcept {
    if(env<1.e-8f && (releasing || velocity==0) && clickEnv<1.e-8f) return 0;
    if(age>=length) releasing=true;
    env+=( (releasing?0.f:velocity)-env)*static_cast<float>(1-std::exp(-1/(sr*(releasing?.025:.005))));
    phase+=freq/sr; phase-=std::floor(phase); age+=1/sr;
    double body=std::sin(2*pi*phase)+.28*std::sin(4*pi*phase)*std::exp(-age*8)+.09*std::sin(6*pi*phase);
    if(timbre==1) body=(std::sin(2*pi*phase)+.45*std::sin(4*pi*phase)+.2*std::sin(8*pi*phase))*std::exp(-age*4);
    if(timbre==2) body=(std::sin(2*pi*phase)+.35*std::sin(4*pi*phase)+.12*std::sin(6*pi*phase))*std::exp(-age*2);
    clickPhase+=1400/sr; clickEnv*=static_cast<float>(std::exp(-1/(.009*sr)));
    return static_cast<float>(body)*env*.32f+static_cast<float>(std::sin(2*pi*clickPhase))*clickEnv;
}
void Scheduler::submit(const Phrase& p) noexcept {
    if(p.epoch!=epoch) return;
    const size_t slot=static_cast<size_t>(p.bar%4); phrases[slot]=p; valid[slot]=true;
}

Arrangement Arranger::generate(int64_t bar,const State& state,Settings settings,int sectionMode) {
    Arrangement result;
    if(sectionMode>0) result.section=static_cast<Section>(std::clamp(sectionMode-1,0,2));
    else if(state.fill || state.density>5) result.section=Section::Build;
    else if(state.density<1.5f) result.section=Section::Breakdown;
    auto bass=bassist.generate(bar,state,settings);
    result.tracks[0]=bass;
    auto& guitar=result.tracks[1]; auto& piano=result.tracks[2];
    for(auto* phrase : {&guitar,&piano}) {
        phrase->bar=bar; phrase->epoch=state.epoch; phrase->root=bass.root;
        phrase->third=bass.third; phrase->fifth=bass.fifth;
    }
    // Lower guitar voicing leaves the bass register free. Piano answers above it.
    const std::array<int,3> guitarChord{48+bass.root,48+bass.root+bass.fifth,60+bass.root};
    const std::array<int,3> pianoChord{60+bass.root+bass.third,60+bass.root+bass.fifth,72+bass.root};
    auto addChord=[&](Phrase& phrase,const auto& chord,double offset,double duration,float velocity,bool strum) {
        for(size_t voice=0;voice<chord.size();++voice) {
            const double delay=strum?voice*.025:0;
            phrase.notes[static_cast<size_t>(phrase.count++)]={bar*4.+offset+delay,duration,chord[voice],velocity};
        }
    };
    float velocity=std::clamp(.3f+state.level*1.5f,.3f,.75f);
    if(result.section==Section::Breakdown) {
        addChord(piano,pianoChord,0,2.8,velocity*.8f,false);
        return result;
    }
    int strokes=result.section==Section::Build?4:2;
    for(int stroke=0;stroke<strokes;++stroke) {
        double offset=stroke*4./strokes;
        if(settings.genre==Genre::Funk) offset+=.5;
        if(settings.genre==Genre::Shuffle) offset+=2./3;
        if(state.fill && offset>=2) continue;
        addChord(guitar,guitarChord,offset,settings.genre==Genre::Funk?.22:.65,velocity,true);
    }
    if(!state.fill) {
        if(settings.density>.65f || result.section==Section::Build) {
            // A short arpeggiated answer develops over a four-bar motif.
            for(int step=0;step<4;++step) {
                int voice=static_cast<int>((step+bar%3)%3);
                piano.notes[static_cast<size_t>(piano.count++)]={bar*4.+.75+step*.75,.45,pianoChord[static_cast<size_t>(voice)],velocity*.85f};
            }
        } else addChord(piano,pianoChord,1.5,1.4,velocity*.75f,false);
    }
    return result;
}
void PolySynth::prepare(double sampleRate,int timbre) {
    nextVoice=0;
    for(auto& voice:voices) {voice.prepare(sampleRate);voice.setTimbre(timbre);}
}
void PolySynth::note(int pitch,float velocity,double duration) noexcept {
    voices[nextVoice].note(pitch,velocity,duration);
    nextVoice=(nextVoice+1)%voices.size();
}
void PolySynth::release() noexcept {for(auto& voice:voices) voice.release();}
float PolySynth::process() noexcept {
    float result=0; for(auto& voice:voices) result+=voice.process(); return result*.35f;
}
void BandPerformance::prepare(double sampleRate) {
    bassVoice.prepare(sampleRate);clickVoice.prepare(sampleRate);guitarVoice.prepare(sampleRate,1);pianoVoice.prepare(sampleRate,2);
    schedulers[1].setFallbackEnabled(false);schedulers[2].setFallbackEnabled(false);
    reset(0);
}
void BandPerformance::reset(uint32_t nextEpoch) {epoch=nextEpoch;for(auto& scheduler:schedulers) scheduler.reset(epoch);harmonyBars.fill(-1);release();}
void BandPerformance::submit(const Arrangement& arrangement) noexcept {
    if(arrangement.tracks[0].epoch!=epoch) return;
    for(size_t track=0;track<schedulers.size();++track) schedulers[track].submit(arrangement.tracks[track]);
    auto bar=arrangement.tracks[0].bar;
    auto slot=static_cast<size_t>(bar%4);
    harmonyBars[slot]=bar;roots[slot]=arrangement.tracks[0].root;sections[slot]=static_cast<int>(arrangement.section);
}
int BandPerformance::rootAt(double beat) const noexcept {
    auto bar=static_cast<int64_t>(beat/4);auto slot=static_cast<size_t>(bar%4);
    return harmonyBars[slot]==bar?roots[slot]:-1;
}
int BandPerformance::sectionAt(double beat) const noexcept {
    auto bar=static_cast<int64_t>(beat/4);auto slot=static_cast<size_t>(bar%4);
    return harmonyBars[slot]==bar?sections[slot]:-1;
}
void BandPerformance::release() noexcept {bassVoice.release();guitarVoice.release();pianoVoice.release();}
float BandPerformance::process(const std::array<float,3>& volumes) noexcept {
    return clickVoice.process()+bassVoice.process()*volumes[0]+guitarVoice.process()*volumes[1]+pianoVoice.process()*volumes[2];
}
}
