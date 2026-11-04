#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>

namespace band {
constexpr double pi = 3.14159265358979323846;
template<class T, size_t N> class Spsc {
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::atomic<size_t>::is_always_lock_free);
    std::array<T, N> data{};
    alignas(64) std::atomic<size_t> read{0};
    alignas(64) std::atomic<size_t> write{0};
public:
    bool push(const T& value) noexcept {
        auto w = write.load(std::memory_order_relaxed);
        auto next = (w + 1) % N;
        if (next == read.load(std::memory_order_acquire)) return false;
        data[w] = value; write.store(next, std::memory_order_release); return true;
    }
    bool pop(T& value) noexcept {
        auto r = read.load(std::memory_order_relaxed);
        if (r == write.load(std::memory_order_acquire)) return false;
        value = data[r]; read.store((r + 1) % N, std::memory_order_release); return true;
    }
    // Only while producer and consumer are stopped.
    void reset() noexcept { read.store(0); write.store(0); }
};
struct Feature { double time=0; float rms=0, low=0, body=0, peak=0; };
class FeatureExtractor {
    double sr=48000, aLow=0, aBody=0;
    float low=0, body=0, peak=0;
    double energy=0, lowEnergy=0, bodyEnergy=0;
    int count=0, hop=240;
public:
    void prepare(double sampleRate);
    bool process(float sample, double time, Feature& result) noexcept;
};
struct State {
    double time=0, beat=0, bpm=110;
    float confidence=0, level=0, density=0;
    bool active=false, locked=false, fill=false;
    uint16_t pattern=0;
    uint32_t epoch=0;
};
class Tracker {
    struct Onset { double time; float weight; };
    std::array<Onset, 128> onsets{};
    int count=0;
    State state;
    float previous=0, average=0, peakWeight=0;
    double lastOnset=-100, lastStrong=-100, lastEstimate=-100, firstOnset=0;
    double pendingTempo=0;
    int pendingCount=0;
    float gate=0.006f;
    void onset(double time, float weight);
    void estimate(double time);
public:
    State process(const Feature& feature);
    void reset(double time=0, double bpm=110);
    void tap(double time, double bpm);
    void setGate(float value) { gate=value; }
    const State& current() const { return state; }
};
enum class Genre { Rock, Funk, Shuffle };
struct Settings { int key=-1; Genre genre=Genre::Rock; float density=.55f, variation=.35f; };
struct Note { double beat=0, duration=.5; int pitch=36; float velocity=.7f; };
struct Phrase {
    std::array<Note, 32> notes{};
    int count=0, root=0;
    int64_t bar=0;
    uint32_t epoch=0;
    int third=3, fifth=7;
};
class Generator {
    uint32_t seed=42;
    std::array<int, 4> progression{0,5,3,4};
    std::array<int, 16> motif{};
    int autoKey=0;
    float random();
public:
    Generator();
    Phrase generate(int64_t bar, const State& state, Settings settings);
};
// Rendering owns this clock. Updates are projections, never wall-clock scheduling.
class BeatClock {
    double beat=0, bpm=110;
    bool running=false;
    uint32_t epoch=0;
public:
    void reset() { beat=0; bpm=110; running=false; epoch=0; }
    void follow(const State& state, double now);
    double advance(double sr) noexcept { double b=beat; if(running) beat+=bpm/(60*sr); return b; }
    double position() const { return beat; }
    bool active() const { return running; }
};
class Synth {
    double sr=48000, phase=0, freq=100, age=0, length=0;
    float velocity=0, env=0, clickEnv=0;
    double clickPhase=0;
    bool releasing=false;
    int timbre=0;
public:
    void setTimbre(int value) noexcept { timbre=value; }
    void prepare(double sampleRate) { sr=sampleRate; silence(); }
    void silence() noexcept { env=0; velocity=0; clickEnv=0; }
    void note(int pitch, float vel, double seconds) noexcept;
    void release() noexcept { releasing=true; }
    void click(bool accent) noexcept { clickEnv=accent?.35f:.22f; clickPhase=0; }
    float process() noexcept;
};
// Fixed-size musical-time schedule: tempo changes do not invalidate future notes.
class Scheduler {
    std::array<Phrase, 4> phrases{};
    std::array<bool, 4> valid{};
    double lastBeat=-1;
    int64_t lastClick=-1;
    int64_t lastFallback=-1;
    int fallbackPitch=36;
    uint32_t epoch=0;
    bool fallbackEnabled=true;
public:
    void setFallbackEnabled(bool enabled) noexcept { fallbackEnabled=enabled; }
    void reset(uint32_t e=0) { valid.fill(false); lastBeat=-1; lastClick=-1; lastFallback=-1; fallbackPitch=36; epoch=e; }
    void submit(const Phrase& p) noexcept;
    template<class Sink> void tick(double beat, double bpm, bool click, bool bass, Sink&& sink) noexcept {
        // Joining mid-bar must not fire all of the already elapsed notes.
        if(lastBeat<0) lastBeat=beat-1.e-8;
        if(beat < lastBeat) { lastBeat=beat; return; }
        const auto integer=static_cast<int64_t>(std::floor(beat));
        if(integer>lastClick) { if(click) sink(-1, integer%4==0 ? 1.f:.6f, .03); lastClick=integer; }
        bool hasCurrent=false;
        for(size_t k=0;k<phrases.size();++k) if(valid[k]) {
            auto& p=phrases[k];
            if(beat>=p.bar*4 && beat<(p.bar+1)*4) {hasCurrent=true;fallbackPitch=36+p.root;}
            for(int i=0;i<p.count;++i) {
                const auto& n=p.notes[i];
                if(bass && n.beat>lastBeat && n.beat<=beat) sink(n.pitch,n.velocity,n.duration*60/bpm);
            }
            if(beat>=4*(p.bar+1)) valid[k]=false;
        }
        if(bass && fallbackEnabled && !hasCurrent && integer>lastFallback) sink(fallbackPitch,.4f,.25*60/bpm);
        lastFallback=integer;
        lastBeat=beat;
    }
};

enum class Section { Groove, Build, Breakdown };
struct Arrangement {
    std::array<Phrase,3> tracks; // Bass, guitar, piano. One shared harmony per bar.
    Section section=Section::Groove;
};
class Arranger {
    Generator bassist;
public:
    Arrangement generate(int64_t bar, const State& state, Settings settings, int sectionMode=0);
};
class PolySynth {
    std::array<Synth,8> voices;
    size_t nextVoice=0;
public:
    void prepare(double sampleRate,int timbre);
    void note(int pitch,float velocity,double duration) noexcept;
    void release() noexcept;
    float process() noexcept;
};
class BandPerformance {
    std::array<Scheduler,3> schedulers;
    Synth bassVoice;
    Synth clickVoice;
    PolySynth guitarVoice,pianoVoice;
    std::array<int64_t,4> harmonyBars{-1,-1,-1,-1};
    std::array<int,4> roots{},sections{};
    uint32_t epoch=0;
public:
    void prepare(double sampleRate);
    void reset(uint32_t epoch);
    void submit(const Arrangement& arrangement) noexcept;
    void release() noexcept;
    int rootAt(double beat) const noexcept;
    int sectionAt(double beat) const noexcept;
    float process(const std::array<float,3>& volumes) noexcept;
    template<class Sink> void tick(double beat,double bpm,bool click,const std::array<bool,3>& enabled,Sink&& sink) noexcept {
        if(!enabled[0]) bassVoice.release();
        if(!enabled[1]) guitarVoice.release();
        if(!enabled[2]) pianoVoice.release();
        for(int track=0;track<3;++track) {
            schedulers[static_cast<size_t>(track)].tick(beat,bpm,click && track==0,enabled[static_cast<size_t>(track)],
                [&](int pitch,float velocity,double duration) {
                    if(pitch<0) clickVoice.click(velocity>.8f);
                    else {
                        if(track==0) bassVoice.note(pitch,velocity,duration);
                        if(track==1) guitarVoice.note(pitch,velocity,duration);
                        if(track==2) pianoVoice.note(pitch,velocity,duration);
                        sink(track,pitch,velocity,duration);
                    }
                });
        }
    }
};
}
