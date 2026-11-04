#include "Core.h"
#include "Fixtures.h"
#include <iostream>
int main() {
    fixtures::Drums drums; drums.section(.2,12,120,120);
    for(double rate : {44100.,48000.,88200.,96000.,192000.}) {
        band::FeatureExtractor extractor; extractor.prepare(rate);
        band::Tracker tracker; band::Feature feature;
        band::Synth synth; synth.prepare(rate); synth.note(40,.8f,.2);
        int frames=0;
        for(int64_t i=0;i<static_cast<int64_t>(12*rate);++i) {
            if(extractor.process(drums.sample(i/rate),i/rate,feature)) {tracker.process(feature);++frames;}
            if(!std::isfinite(synth.process())) return 1;
        }
        auto s=tracker.current();
        std::cout<<rate<<" Hz: "<<s.bpm<<" BPM, "<<frames<<" feature frames\n";
        if(!s.active || !s.locked || std::abs(s.bpm-120)>1) return 1;
    }
    return 0;
}
