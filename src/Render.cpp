#include "Core.h"
#include "../tests/Fixtures.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <fstream>

int main(int argc,char** argv) {
    if(argc<3) { std::cout<<"drum_render input.wav|--demo output.wav [seconds for demo]\nWrites isolated bass/click audio, .mid and .csv alongside output.\n"; return 1; }
    const double sr=48000;
    juce::AudioBuffer<float> input;
    fixtures::Drums demo;
    if(std::string(argv[1])=="--demo") {
        double seconds=argc>3?std::stod(argv[3]):30;
        if(seconds<=0 || seconds>600) {std::cerr<<"Duration must be 0..600 seconds\n";return 1;}
        demo.section(.2,seconds,110,120,2,false,true); input.setSize(1,static_cast<int>(seconds*sr));
        for(int i=0;i<input.getNumSamples();++i) input.setSample(0,i,demo.sample(i/sr));
    } else {
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(juce::String::fromUTF8(argv[1]))));
        if(!reader || reader->lengthInSamples>reader->sampleRate*600) { std::cerr<<"Unreadable file or >10 minutes\n"; return 1; }
        juce::AudioBuffer<float> raw(1,static_cast<int>(reader->lengthInSamples));
        if(!reader->read(&raw,0,raw.getNumSamples(),0,true,false)) return 1;
        input.setSize(1,static_cast<int>(raw.getNumSamples()*sr/reader->sampleRate));
        for(int i=0;i<input.getNumSamples();++i) {
            double pos=i*reader->sampleRate/sr; int j=static_cast<int>(pos); float f=static_cast<float>(pos-j);
            input.setSample(0,i,raw.getSample(0,j)*(1-f)+raw.getSample(0,std::min(j+1,raw.getNumSamples()-1))*f);
        }
    }
    auto out=juce::File(juce::String::fromUTF8(argv[2]));
    out.deleteFile(); auto stream=out.createOutputStream(); if(!stream) return 1;
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),sr,1,24,{},0));
    if(!writer) return 1;
    std::ofstream csv(out.withFileExtension("csv").getFullPathName().toStdString());
    csv<<"time,bpm,beat,confidence,active,fill\n";
    juce::MidiMessageSequence sequence;
    auto tempo=juce::MidiMessage::tempoMetaEvent(500000); tempo.setTimeStamp(0); sequence.addEvent(tempo);
    band::FeatureExtractor extractor; extractor.prepare(sr); band::Tracker tracker; band::Feature f;
    band::BeatClock clock; band::Scheduler scheduler; band::Synth synth; synth.prepare(sr); band::Generator generator;
    uint32_t epoch=0; int64_t planned=-1; juce::AudioBuffer<float> buffer(1,128);
    for(int block=0;block<input.getNumSamples();block+=128) {
        int n=std::min(128,input.getNumSamples()-block);
        for(int i=0;i<n;++i) {
            double time=(block+i)/sr;
            if(extractor.process(input.getSample(0,block+i),time,f)) {
                auto s=tracker.process(f); clock.follow(s,time);
                if(s.epoch!=epoch) {epoch=s.epoch; scheduler.reset(epoch); planned=-1;}
                if(s.active && s.locked) {
                    int64_t bar=static_cast<int64_t>(s.beat/4); if(planned<bar-1) planned=bar-1;
                    while(planned<bar+2) scheduler.submit(generator.generate(++planned,s,{}));
                } else synth.release();
                csv<<time<<","<<s.bpm<<","<<s.beat<<","<<s.confidence<<","<<s.active<<","<<s.fill<<"\n";
            }
            if(clock.active()) scheduler.tick(clock.advance(sr),tracker.current().bpm,true,true,[&](int p,float v,double d){
                if(p<0) synth.click(v>.8); else {
                    synth.note(p,v,d);
                    auto on=juce::MidiMessage::noteOn(1,p,v); on.setTimeStamp(time*1920); sequence.addEvent(on);
                    auto off=juce::MidiMessage::noteOff(1,p); off.setTimeStamp((time+d)*1920); sequence.addEvent(off);
                }
            });
            buffer.setSample(0,i,synth.process()*.6f);
        }
        if(!writer->writeFromAudioSampleBuffer(buffer,0,n)) return 1;
    }
    writer.reset();
    sequence.updateMatchedPairs(); juce::MidiFile midi; midi.setTicksPerQuarterNote(960); midi.addTrack(sequence);
    auto mid=out.withFileExtension("mid"); mid.deleteFile(); auto midiStream=mid.createOutputStream();
    if(!midiStream || !midi.writeTo(*midiStream)) return 1;
    if(!demo.hits.empty()) {
        auto drumFile=out.getSiblingFile(out.getFileNameWithoutExtension()+"-drums.wav"); drumFile.deleteFile();
        auto ds=drumFile.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> dw(format.createWriterFor(ds.release(),sr,1,24,{},0));
        if(dw) dw->writeFromAudioSampleBuffer(input,0,input.getNumSamples());
    }
    std::cout<<"Rendered "<<input.getNumSamples()/sr<<" seconds; final BPM "<<tracker.current().bpm<<"\n";
}
