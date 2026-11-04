#include "Core.h"
#include <iostream>
#include <stdexcept>

void require(bool condition,const char* message) {
    if(!condition) throw std::runtime_error(message);
}
int main() {
    try {
        band::State state;
        state.active=state.locked=true;state.epoch=4;state.density=4;state.level=.1f;
        band::Arranger arranger;
        for(int key=0;key<12;++key) for(int bar=0;bar<16;++bar) {
            band::Settings settings;settings.key=key;settings.density=.8f;
            auto arrangement=arranger.generate(bar,state,settings,2);
            for(const auto& phrase:arrangement.tracks) {
                require(phrase.root==arrangement.tracks[0].root,"shared root");
                require(phrase.epoch==state.epoch && phrase.bar==bar,"shared timeline");
                require(phrase.count>0 && phrase.count<=32,"bounded notes");
                for(int i=0;i<phrase.count;++i) {
                    const auto& note=phrase.notes[static_cast<size_t>(i)];
                    require(note.beat>=bar*4 && note.beat<(bar+1)*4,"notes in bar");
                    require(note.pitch>=0 && note.pitch<=127 && note.duration>0,"valid note");
                }
            }
        }
        auto groove=arranger.generate(0,state,{},1);
        auto breakdown=arranger.generate(0,state,{},3);
        require(breakdown.tracks[1].count==0 && breakdown.tracks[2].count==3,"breakdown leaves space");
        state.fill=true;
        auto fill=arranger.generate(0,state,{},2);
        require(fill.tracks[1].count<groove.tracks[1].count+1 && fill.tracks[2].count==0,"fill response");
        state.fill=false;
        band::BandPerformance performance;performance.prepare(48000);performance.reset(state.epoch);
        performance.submit(groove);
        std::array<int,3> notes{};
        float peak=0;
        for(int i=0;i<96000;++i) {
            performance.tick(i/24000.,120,false,{true,true,true},[&](int track,int,float,double){++notes[static_cast<size_t>(track)];});
            float sample=performance.process({1,.7f,.7f});
            require(std::isfinite(sample),"finite ensemble output");peak=std::max(peak,std::abs(sample));
        }
        for(size_t track=0;track<3;++track) require(notes[track]==groove.tracks[track].count,"each note exactly once");
        require(peak>.01f && peak<.95f,"audible bounded mix");
        performance.reset(state.epoch+1);performance.submit(groove);
        require(performance.rootAt(0)==-1,"stale harmony rejected");
        performance.release();
        for(int i=0;i<48000;++i) performance.process({1,1,1});
        require(std::abs(performance.process({1,1,1}))<1.e-7f,"release reaches silence");
        performance.reset(state.epoch);performance.submit(groove);notes.fill(0);
        for(int i=0;i<96000;++i) performance.tick(i/24000.,120,false,{false,true,false},[&](int track,int,float,double){++notes[static_cast<size_t>(track)];});
        require(notes[0]==0 && notes[1]>0 && notes[2]==0,"instrument toggles independent");
        std::cout<<"PASS ensemble harmony, roles, fill, scheduling, mix, mute and stale epochs\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 1;}
}
