#include "Core.h"
#include "Fixtures.h"
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

void evaluate(const std::string& name, fixtures::Drums drums, double bleed=0) {
    band::Tracker tracker; band::FeatureExtractor extractor; extractor.prepare(48000); band::Feature f;
    std::vector<double> phases, outputPhases; double bpmError=0, acquisition=-1; int locked=0, total=0;
    band::BeatClock clock;
    size_t reference=0;
    for(int64_t i=0;i<static_cast<int64_t>(drums.duration*48000);++i) {
        double t=i/48000.;
        // Deliberately adversarial 140 BPM bass/PA-like pulse, independent of drummer.
        double age=std::fmod(t,60./140);
        float x=drums.sample(t)+static_cast<float>(bleed*std::exp(-age*12)*std::sin(2*band::pi*82.4*age));
        clock.advance(48000);
        if(!extractor.process(x,t,f)) continue;
        auto s=tracker.process(f); clock.follow(s,t);
        if(s.locked && acquisition<0) acquisition=t;
        while(reference+1<drums.beats.size() && drums.beats[reference+1]<=t) ++reference;
        if(t<6 || reference+1>=drums.beats.size()) continue;
        ++total;
        if(!s.locked || !s.active) continue;
        ++locked;
        double period=drums.beats[reference+1]-drums.beats[reference];
        double expected=reference+(t-drums.beats[reference])/period;
        double error=s.beat-expected; error-=std::round(error);
        phases.push_back(std::abs(error)*period*1000);
        double outputError=clock.position()-expected; outputError-=std::round(outputError);
        outputPhases.push_back(std::abs(outputError)*period*1000);
        bpmError+=std::abs(s.bpm-60/period);
    }
    std::sort(phases.begin(),phases.end()); std::sort(outputPhases.begin(),outputPhases.end());
    std::cout<<name<<","<<acquisition<<","<<double(locked)/std::max(1,total)<<","<<bpmError/std::max(1,locked)<<",";
    if(phases.empty()) std::cout<<"nan,nan";
    else std::cout<<phases[phases.size()/2]<<","<<phases[phases.size()*95/100];
    std::cout<<","<<tracker.current().bpm<<","<<(outputPhases.empty()?0:outputPhases[outputPhases.size()*95/100])<<"\n";
}
int main() {
    std::cout<<std::fixed<<std::setprecision(3)<<"case,first_lock_seconds,locked_fraction_after_6s,bpm_mae,phase_ms_p50,phase_ms_p95,final_bpm,clock_phase_ms_p95\n";
    for(int bpm : {60,90,120,150,180}) {
        fixtures::Drums d; d.section(.2,24,bpm,bpm); evaluate("straight_"+std::to_string(bpm),d);
    }
    fixtures::Drums sixteen; sixteen.section(.2,24,120,120,4); evaluate("sixteenths",sixteen);
    fixtures::Drums shuffle; shuffle.section(.2,24,100,100,2,true); evaluate("shuffle",shuffle);
    fixtures::Drums ramp; ramp.section(.2,30,100,130); evaluate("ramp_100_130",ramp);
    fixtures::Drums step; step.section(.2,12,90,90); step.section(12.2,18,130,130); evaluate("step_90_130",step);
    fixtures::Drums fill; fill.section(.2,24,120,120,2,false,true); evaluate("fills",fill);
    fixtures::Drums jitter; jitter.section(.2,24,120,120);
    for(auto& h:jitter.hits) h.time+=.009*std::sin(h.time*7.3);
    for(auto& b:jitter.beats) b+=.009*std::sin(b*7.3);
    std::sort(jitter.hits.begin(),jitter.hits.end(),[](auto a,auto b){return a.time<b.time;});
    evaluate("microtiming_9ms",jitter);
    fixtures::Drums bleed; bleed.section(.2,24,120,120); evaluate("bleed_140bpm_level_0.1",bleed,.1); evaluate("bleed_140bpm_level_0.8",bleed,.8);
}
