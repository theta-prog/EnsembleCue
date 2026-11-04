#include "Engine.h"
#include <iostream>
#include <fstream>
#include <windows.h>
#include <psapi.h>

int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    Engine engine;
    int seconds=argc>1?std::stoi(argv[1]):30;
    if(seconds<1 || seconds>3600) return 1;
    if(argc>2 && std::string(argv[2])=="--asio") {
        engine.devices.setCurrentAudioDeviceType("ASIO",true);
        auto setup=engine.devices.getAudioDeviceSetup();
        setup.inputDeviceName=setup.outputDeviceName="Yamaha Steinberg USB ASIO";
        setup.sampleRate=48000; setup.bufferSize=128;
        auto error=engine.devices.setAudioDeviceSetup(setup,true);
        if(error.isNotEmpty()) {std::cerr<<error<<"\n";return 1;}
    }
    engine.volume=0; engine.source=1; engine.bass=true;
    auto* dev=engine.devices.getCurrentAudioDevice();
    if(!dev) {std::cerr<<engine.getError()<<"\n";return 1;}
    std::cout<<"Device: "<<dev->getName()<<" / "<<dev->getTypeName()<<"\n";
    engine.startSession();
    juce::File output=juce::File::getCurrentWorkingDirectory().getChildFile("results"); output.createDirectory();
    std::ofstream log(output.getChildFile("audio-probe.csv").getFullPathName().toStdString());
    log<<"second,bpm,confidence,locked,working_set_bytes,private_bytes,process_cpu_seconds,callback_count,feature_drops,xruns\n";
    double startCpu=0,finalCpu=0; SIZE_T initialPrivate=0,finalPrivate=0;
    uint64_t previousCallbacks=0; int stalledSeconds=0;
    for(int i=0;i<=seconds;++i) {
        PROCESS_MEMORY_COUNTERS_EX memory{}; memory.cb=sizeof(memory);
        GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory));
        FILETIME create,exit,kernel,user; GetProcessTimes(GetCurrentProcess(),&create,&exit,&kernel,&user);
        ULARGE_INTEGER k{},u{}; k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
        double cpu=(k.QuadPart+u.QuadPart)*1e-7;
        if(i==0) {startCpu=cpu;initialPrivate=memory.PrivateUsage;} finalCpu=cpu;finalPrivate=memory.PrivateUsage;
        auto callbacks=engine.callbackCount.load();
        if(i>0 && callbacks==previousCallbacks) ++stalledSeconds;
        previousCallbacks=callbacks;
        log<<i<<","<<engine.bpm.load()<<","<<engine.confidence.load()<<","<<engine.locked.load()<<","<<memory.WorkingSetSize<<","<<memory.PrivateUsage<<","<<cpu<<","<<callbacks<<","<<engine.droppedFeatures.load()<<","<<dev->getXRunCount()<<"\n";
        log.flush();
        if(i<seconds) juce::MessageManager::getInstance()->runDispatchLoopUntil(1000);
    }
    engine.exportMetrics(output.getChildFile("audio-device-metrics.csv"));
    std::cout<<"BPM "<<engine.bpm<<"; locked "<<engine.locked<<"; overruns "<<engine.overruns<<"; feature drops "<<engine.droppedFeatures<<"\n";
    std::cout<<"CPU seconds "<<finalCpu-startCpu<<"; private bytes delta "<<static_cast<int64_t>(finalPrivate)-static_cast<int64_t>(initialPrivate)<<"\n";
    std::cout<<"Stalled seconds "<<stalledSeconds<<"\n";
    bool ok=engine.locked && engine.droppedFeatures==0 && stalledSeconds==0;
    engine.stopSession(); return ok?0:2;
}
