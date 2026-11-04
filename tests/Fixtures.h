#pragma once
#include "Core.h"
#include <vector>
namespace fixtures {
struct Hit { double time; float amplitude; int type; };
struct Drums {
    std::vector<Hit> hits;
    std::vector<double> beats;
    double duration=0;
    void add(double t,float a,int type) { hits.push_back({t,a,type}); }
    void section(double begin,double seconds,double startBpm,double endBpm,int subdivision=2,bool shuffle=false,bool fill=false) {
        int index=0;
        for(double t=begin;t<begin+seconds;) {
            const double bpm=startBpm+(endBpm-startBpm)*(t-begin)/seconds;
            const double period=60/bpm;
            beats.push_back(t); add(t,.7f,index%2);
            for(int j=1;j<subdivision;++j) add(t+period*(shuffle?2./3:double(j)/subdivision),.1f,2);
            if(fill && index%16>=14) for(int j=1;j<4;++j) add(t+period*j/4,.35f,1);
            ++index; t+=period;
        }
        duration=std::max(duration,begin+seconds);
        std::sort(hits.begin(),hits.end(),[](auto a,auto b){return a.time<b.time;});
    }
    float sample(double time) const {
        auto i=std::lower_bound(hits.begin(),hits.end(),time-.09,[](auto h,double t){return h.time<t;});
        double value=0;
        for(;i!=hits.end() && i->time<=time;++i) {
            double age=time-i->time;
            double freq=i->type==0?70:(i->type==1?280:7000);
            value+=i->amplitude*std::exp(-age*(i->type==2?160:65))*std::sin(2*band::pi*freq*age);
        }
        return static_cast<float>(value);
    }
};
}
