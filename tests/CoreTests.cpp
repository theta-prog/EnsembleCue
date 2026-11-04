#include "Core.h"
#include "Fixtures.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <string>
using namespace band;
static int assertions=0;
void check(bool value,const std::string& name) { ++assertions; if(!value) throw std::runtime_error(name); }
State feed(const fixtures::Drums& drums,double seconds,Tracker& tracker) {
    FeatureExtractor extractor; extractor.prepare(48000); Feature f;
    for(int64_t i=0;i<static_cast<int64_t>(seconds*48000);++i)
        if(extractor.process(drums.sample(i/48000.),i/48000.,f)) tracker.process(f);
    return tracker.current();
}
int main() {
    try {
        for(int bpm : {60,75,90,110,120,150,180}) for(int sub : {2,4}) {
            fixtures::Drums drums; drums.section(.2,16,bpm,bpm,sub);
            Tracker tracker; auto s=feed(drums,16,tracker);
            std::cout<<"steady,"<<bpm<<","<<sub<<","<<s.bpm<<","<<s.confidence<<"\n";
            check(s.active && s.locked,"steady lock"); check(std::abs(s.bpm-bpm)<3,"steady tempo "+std::to_string(bpm));
        }
        fixtures::Drums shuffle; shuffle.section(.2,16,100,100,2,true);
        Tracker st; check(std::abs(feed(shuffle,16,st).bpm-100)<3,"shuffle");
        fixtures::Drums ramp; ramp.section(.2,30,100,130);
        Tracker rt; auto r=feed(ramp,30,rt); std::cout<<"ramp,"<<r.bpm<<"\n";
        check(std::abs(r.bpm-130)<7,"ramp follows");
        fixtures::Drums step; step.section(.2,12,90,90); step.section(12.2,18,130,130);
        Tracker tt; auto t=feed(step,30,tt); std::cout<<"step,"<<t.bpm<<"\n";
        check(std::abs(t.bpm-130)<4,"step follows");
        fixtures::Drums fill; fill.section(.2,24,120,120,2,false,true);
        Tracker ft; check(std::abs(feed(fill,24,ft).bpm-120)<4,"fill preserves tempo");
        fixtures::Drums human; human.section(.2,24,120,120,2);
        for(auto& hit:human.hits) hit.time+=.009*std::sin(hit.time*7.3);
        std::sort(human.hits.begin(),human.hits.end(),[](auto a,auto b){return a.time<b.time;});
        Tracker ht; check(std::abs(feed(human,24,ht).bpm-120)<4,"9ms microtiming");
        ht.tap(24,90); check(ht.current().active && ht.current().bpm==90 && ht.current().beat==0,"tap phase and tempo");
        Tracker silence; fixtures::Drums empty; check(!feed(empty,3,silence).active,"silence inactive");
        silence.tap(3,100); check(!silence.current().active,"tap without input stays silent");
        fixtures::Drums stopped; stopped.section(.2,6,120,120); Tracker sp;
        check(!feed(stopped,10,sp).active,"stop detection");
        stopped.section(11,12,90,90); Tracker restart;
        check(std::abs(feed(stopped,23,restart).bpm-90)<4,"restart acquisition");
        Spsc<int,32> queue;
        std::thread producer([&]{for(int i=0;i<100000;++i) while(!queue.push(i)) std::this_thread::yield();});
        for(int i=0;i<100000;++i) { int n; while(!queue.pop(n)) std::this_thread::yield(); check(n==i,"SPSC order"); }
        producer.join();
        State s; s.active=s.locked=true; s.confidence=1; s.bpm=120; s.epoch=2; s.level=.15f; s.density=4;
        Generator gen; Scheduler scheduler; scheduler.reset(2);
        auto phrase=gen.generate(0,s,{}); scheduler.submit(phrase);
        check(phrase.count>=4,"phrase anchors");
        int notes=0; double beat=0;
        for(int i=0;i<96000;++i) { scheduler.tick(beat,120,false,true,[&](int p,float,double){if(p>=0)++notes;}); beat+=2./48000; }
        check(notes==phrase.count,"exactly once note scheduling");
        scheduler.reset(3); scheduler.submit(phrase); notes=0;
        scheduler.tick(4,120,false,true,[&](int p,float v,double){if(v!=.4f || p!=36) ++notes;}); check(notes==0,"stale generation discarded / fallback root");
        scheduler.reset(2); scheduler.submit(phrase); notes=0;
        scheduler.tick(2.1,120,false,true,[&](int,float,double){++notes;}); check(notes==0,"join skips elapsed notes");
        scheduler.tick(4.1,120,false,true,[&](int,float,double){++notes;}); check(notes>=1,"generator starvation fallback");
        BeatClock clock; clock.follow(s,0); double before=clock.position();
        for(int i=0;i<48000;++i) clock.advance(48000);
        check(std::abs(clock.position()-before-2)<1e-8,"sample clock");
        s.time=1; s.beat=1.9; clock.follow(s,1); check(clock.position()>=before+1.99,"phase never rewinds");
        clock.follow(s,2); check(!clock.active(),"stale worker mutes");
        Synth synth; synth.prepare(48000); synth.note(36,.8f,.2);
        for(int i=0;i<48000;++i) check(std::isfinite(synth.process()),"finite synth");
        std::cout<<"PASS "<<assertions<<" assertions\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<"\n"; return 1; }
}
