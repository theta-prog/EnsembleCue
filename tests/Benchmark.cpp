#include "Core.h"
#include "Fixtures.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip>
int main(int argc,char** argv) {
    double seconds=argc>1?std::stod(argv[1]):60;
    fixtures::Drums drums; drums.section(.2,seconds,120,120);
    band::FeatureExtractor extractor; extractor.prepare(48000); band::Tracker tracker; band::Feature f;
    band::Synth synth; synth.prepare(48000); band::BeatClock clock; band::Scheduler scheduler;
    band::Generator generator; int64_t planned=-1; uint32_t epoch=0;
    std::vector<double> blockTimes,phaseErrors; volatile float sink=0;
    const auto start=std::chrono::steady_clock::now();
    double bpmError=0; size_t measurements=0;
    for(int64_t block=0;block<static_cast<int64_t>(seconds*48000);block+=128) {
        float input[128]; for(int j=0;j<128;++j) input[j]=drums.sample((block+j)/48000.);
        auto begin=std::chrono::steady_clock::now();
        for(int j=0;j<128;++j) {
            double time=(block+j)/48000.;
            if(extractor.process(input[j],time,f)) {
                auto s=tracker.process(f); clock.follow(s,time);
                if(s.epoch!=epoch) { epoch=s.epoch; scheduler.reset(epoch); planned=-1; }
                if(s.locked && s.active) {
                    int64_t bar=static_cast<int64_t>(s.beat/4);
                    while(planned<bar+2) scheduler.submit(generator.generate(++planned,s,{}));
                    if(time>8) {
                        bpmError+=std::abs(s.bpm-120); ++measurements;
                        double reference=(time-.2)*2;
                        double delta=s.beat-reference; delta-=std::round(delta);
                        phaseErrors.push_back(std::abs(delta)*500);
                    }
                }
            }
            if(clock.active()) scheduler.tick(clock.advance(48000),tracker.current().bpm,true,true,[&](int p,float v,double d){if(p<0)synth.click(v>.8);else synth.note(p,v,d);});
            sink=synth.process();
        }
        blockTimes.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count());
    }
    auto wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::sort(blockTimes.begin(),blockTimes.end()); std::sort(phaseErrors.begin(),phaseErrors.end());
    std::cout<<std::fixed<<std::setprecision(4)<<"audio_seconds,wall_seconds,block_us_p50,block_us_p99,block_us_max,bpm_mae,phase_ms_p50,phase_ms_p95\n";
    std::cout<<seconds<<","<<wall<<","<<blockTimes[blockTimes.size()/2]<<","<<blockTimes[blockTimes.size()*99/100]<<","<<blockTimes.back()<<","<<bpmError/std::max(size_t{1},measurements)<<",";
    if(phaseErrors.empty()) std::cout<<"0,0\n"; else std::cout<<phaseErrors[phaseErrors.size()/2]<<","<<phaseErrors[phaseErrors.size()*95/100]<<"\n";
    (void)sink;
}
