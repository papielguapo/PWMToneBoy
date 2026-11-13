#pragma once
#include "AnalysisModels.h"

// Editor-owned worker. Neither the audio callback nor mouse handling waits for analysis.
class ToneBoyHarmonicWorker final : private juce::Thread
{
public:
    ToneBoyHarmonicWorker():juce::Thread("Tone Boy harmonic analysis") { startThread(juce::Thread::Priority::low); }
    ~ToneBoyHarmonicWorker() override { signalThreadShouldExit(); wake.signal(); stopThread(-1); }
    bool submit(const std::array<float,8192>& in,const std::array<float,8192>& out,double sampleRate,uint64_t epoch)
    {
        const juce::ScopedTryLock guard(lock);
        if(!guard.isLocked() || busy) return false;
        input=in; output=out; rate=sampleRate; jobEpoch=epoch; busy=true; wake.signal(); return true;
    }
    bool take(ToneBoyHarmonicResult& result,uint64_t epoch)
    {
        const juce::ScopedTryLock guard(lock);
        if(!guard.isLocked() || !ready) return false;
        ready=false;
        if(resultEpoch!=epoch) return false;
        result=completed; return true;
    }
    void waitForDiagnostic() { done.wait(5000); } // Offline checks only.
private:
    void run() override
    {
        while(!threadShouldExit())
        {
            wake.wait(); if(threadShouldExit()) break;
            // Busy prevents the submitter from modifying the input until completion.
            const auto result=ToneBoyHarmonicAnalysis::analyse(input,output,rate);
            { const juce::ScopedLock guard(lock); completed=result; resultEpoch=jobEpoch; ready=true; busy=false; }
            done.signal();
        }
    }
    juce::CriticalSection lock;
    juce::WaitableEvent wake,done;
    std::array<float,8192> input{},output{};
    double rate{}; uint64_t jobEpoch{},resultEpoch{};
    bool busy{},ready{};
    ToneBoyHarmonicResult completed;
};
