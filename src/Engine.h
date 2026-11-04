#pragma once
#include "Core.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

class Engine final : public juce::AudioIODeviceCallback, private juce::Thread {
public:
    Engine();
    ~Engine() override;
    juce::AudioDeviceManager devices;
    std::atomic<bool> playing{false}, bass{false}, click{true};
    std::atomic<int> source{0}, key{-1}, genre{0}, inputChannel{0}; // 0 mic, 1 demo, 2 WAV
    std::atomic<float> volume{.3f}, density{.55f}, variation{.35f}, gate{.006f};
    std::atomic<float> timingOffsetMs{0};
    std::atomic<double> bpm{110}, beat{0}, confidence{0}, level{0}, inputPeak{0}, sampleRate{48000};
    std::atomic<float> onsetDensity{0};
    std::atomic<bool> locked{false}, active{false}, fill{false}, fileEnded{false};
    std::atomic<uint32_t> droppedFeatures{0}, droppedMidi{0}, pattern{0};
    std::atomic<int> bufferSize{0}, inputLatency{0}, outputLatency{0};
    std::atomic<int> initialXruns{-1};
    std::atomic<double> callbackMaxUs{0}, callbackTotalUs{0};
    std::atomic<uint64_t> callbackCount{0}, overruns{0};
    std::atomic<double> tapBpm{0};
    std::atomic<uint32_t> resetRequest{0};
    juce::String error;
    void startSession();
    void stopSession();
    juce::String loadFile(const juce::File&);
    juce::String selectMidi(const juce::String& identifier);
    void exportMetrics(const juce::File&);
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String& message) override;
    void audioDeviceIOCallbackWithContext(const float* const*,int,float* const*,int,int,const juce::AudioIODeviceCallbackContext&) override;
private:
    void run() override;
    struct MidiEvent { int pitch; float velocity; uint32_t epoch; };
    band::Spsc<band::Feature,2048> features;
    band::Spsc<band::State,512> states;
    band::Spsc<band::Phrase,32> phrases;
    band::Spsc<MidiEvent,256> midiEvents;
    band::FeatureExtractor extractor;
    band::Tracker tracker;
    band::Generator generator;
    band::BeatClock clock;
    band::Scheduler scheduler;
    band::Synth synth;
    band::State latest;
    std::unique_ptr<juce::MidiOutput> midi;
    juce::AudioBuffer<float> file;
    double fileRate=48000, filePosition=0, sr=48000, gain=0;
    int64_t samples=0;
    uint32_t audioEpoch=0;
    int midiPitch=-1;
    double midiOffBeat=0;
    double demoPhase=0, demoAge=100, hatAge=100;
    int demoBeat=0;
    bool wasAudible=false;
    juce::CriticalSection errorMutex;
public:
    juce::String getError() { juce::ScopedLock lock(errorMutex); return error; }
};
