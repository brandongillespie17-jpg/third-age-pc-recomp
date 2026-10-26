#include "third_age_present_diagnostics.h"
#include <cassert>
#include <filesystem>
using namespace gbarecomp;
int main(int argc,char** argv){
    assert(argc==2);unsetenv("THIRD_AGE_DIAGNOSTIC_TRACE");ThirdAgePresentDiagnostics off;assert(!off.active());off.record({});off.dump();
    setenv("THIRD_AGE_DIAGNOSTIC_TRACE",argv[1],1);ThirdAgePresentDiagnostics on;assert(on.active());
    ThirdAgeRefreshClock clock(165);ThirdAgePresentSample s;
    ThirdAgePresentDiagnostics::clock_sample(s,clock,100);assert(s.clock_action==1&&clock.deadline_ns()==0&&s.lateness_ns==0);
    clock.presented(100);const auto deadline=clock.deadline_ns(),period=clock.period_ns();
    s={};ThirdAgePresentDiagnostics::clock_sample(s,clock,deadline);assert(s.clock_action==2&&s.lateness_ns==0&&clock.deadline_ns()==deadline);
    s={};ThirdAgePresentDiagnostics::clock_sample(s,clock,deadline+period);assert(s.clock_action==2&&s.missed_periods==1);
    s={};ThirdAgePresentDiagnostics::clock_sample(s,clock,deadline+period+1);assert(s.clock_action==3&&s.missed_periods==1);assert(clock.deadline_ns()==deadline);
    for(size_t i=0;i<ThirdAgePresentDiagnostics::capacity+3;++i){s.guest_frame=i;on.record(s);}
    assert(!std::filesystem::exists(argv[1]));on.dump();assert(std::filesystem::exists(argv[1]));
}
