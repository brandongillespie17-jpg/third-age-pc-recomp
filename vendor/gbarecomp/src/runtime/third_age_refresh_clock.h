#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
namespace gbarecomp {
inline int third_age_present_hz(){
    static const int hz=[](){const char* p=std::getenv("THIRD_AGE_PRESENT_HZ");if(!p)return 0;
        char* end=nullptr;const long n=std::strtol(p,&end,10);
        return end&&!*end&&(n==60||n==120||n==144||n==165)?int(n):0;}();return hz;
}
inline int64_t third_age_present_now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();}
class ThirdAgeRefreshClock {
    int64_t next_=0,period_=0;
public:
    explicit ThirdAgeRefreshClock(int hz=third_age_present_hz()):period_(hz>0?1000000000ll/hz:0){}
    bool enabled()const{return period_!=0;}
    // Read-only diagnostics; scheduling remains entirely in presented().
    int64_t deadline_ns()const{return next_;}
    int64_t period_ns()const{return period_;}
    bool due(int64_t now)const{return !period_||!next_||now>=next_;}
    void presented(int64_t now){
        if(!period_)return;
        if(!next_||now-next_>period_)next_=now+period_;
        else do{next_+=period_;}while(next_<=now);
    }
};
struct ThirdAgeCameraTween {
    static constexpr double guest_ns=280896.0*1e9/16777216.0;
    uint64_t frame=0,key=0,changed_frame=0;int x=0,y=0,old_x=0,old_y=0;
    int64_t stamp=0,last_seen=0;double duration_ns=guest_ns;
    double from_x=0,from_y=0;
    bool valid=false,smooth=false;unsigned reason=0;
    // 0 reset, 1 first/cut, 2 gap, 3 stall, 4 jump, 5 stationary, 6 moving.
    void reset(){valid=smooth=false;reason=0;}
    bool submit(uint64_t f,uint64_t k,int cx,int cy,int64_t now){
        if(valid&&f==frame&&k==key&&cx==x&&cy==y)return false;
        const bool same_scene=valid&&k==key;
        const bool consecutive=same_scene&&f==frame+1;
        const bool timely=consecutive&&now>=last_seen&&now-last_seen<guest_ns*2.5;
        const bool small=std::abs(int64_t(cx)-x)<=16&&std::abs(int64_t(cy)-y)<=16;
        if(timely&&cx==x&&cy==y){
            // Camera updates can run every second guest frame. Keep an active
            // tween through the unchanged frame instead of cancelling it.
            frame=f;last_seen=now;reason=smooth&&alpha(now)<1.?6:5;return true;
        }
        // Re-target from the camera currently on screen, not the prior native
        // target. The prior tween can still be in flight when this step arrives.
        const double displayed_x=x-offset_x(now),displayed_y=y-offset_y(now);
        smooth=timely&&small;
        reason=!same_scene?1:!consecutive?2:!timely?3:!small?4:6;
        // Camera coordinates advance on guest ticks. Host preparation and
        // presentation jitter must not change the speed of the next step.
        const uint64_t ticks=f>=changed_frame?f-changed_frame:0;
        duration_ns=smooth&&ticks>=1&&ticks<=3?guest_ns*double(std::min(ticks,uint64_t(2))):guest_ns;
        changed_frame=f;
        from_x=smooth?displayed_x:cx;from_y=smooth?displayed_y:cy;
        old_x=x;old_y=y;x=cx;y=cy;key=k;frame=f;stamp=last_seen=now;valid=true;
        if(x==old_x&&y==old_y){smooth=false;reason=5;}
        return true;
    }
    double alpha(int64_t now)const{return smooth?std::clamp(double(now-stamp)/duration_ns,0.,1.):1.;}
    double offset_x(int64_t now)const{return smooth?(x-from_x)*(1.-alpha(now)):0.;}
    double offset_y(int64_t now)const{return smooth?(y-from_y)*(1.-alpha(now)):0.;}
};
}// Continuous visual follower for mouse-owned scrolling. Native images remain
// authoritative; position and velocity survive unchanged native camera ticks.
// Two finite averaging windows turn the common two-tick camera staircase into
// constant-speed motion. Unlike a spring chasing each endpoint, this has no
// repeated acceleration/deceleration and finishes within three guest ticks.
// The positive kernel stays inside the observed camera path: no prediction or
// movement beyond streamed terrain. All calculations use elapsed host time.
struct ThirdAgeScrollMotion {
    bool active=false;double x=0,y=0,vx=0,vy=0;int64_t stamp=0;
    struct Step {double time,dx,dy;};
    std::array<Step,32> steps{};unsigned count=0;
    double base_x=0,base_y=0,target_x=0,target_y=0,clock=0,period=0;
    uint64_t sample_frame=0;int64_t sample_stamp=0;
    std::array<double,5> cadence_samples{};unsigned cadence_count=0,cadence_slot=0;
    static constexpr double tick=gbarecomp::ThirdAgeCameraTween::guest_ns/1e9;
    static constexpr double first=2*tick,second=tick,span=first+second;
    // Stretch the finite window only when completed guest frames themselves
    // arrive slowly. A continuous internal clock avoids position jumps when
    // cadence changes; at full speed the window is exactly three native ticks.
    void observe_frame(uint64_t frame,int64_t now){
        if(sample_stamp&&frame>sample_frame&&frame-sample_frame<=3&&now>sample_stamp){
            double dt=double(now-sample_stamp)/1e9/double(frame-sample_frame);
            if(dt<.15){cadence_samples[cadence_slot++%5]=std::clamp(dt,tick,4*tick);cadence_count=std::min(5u,cadence_count+1);
                auto values=cadence_samples;std::sort(values.begin(),values.begin()+cadence_count);
                const double next=values[cadence_count/2];period=period?period+(next-period)*.125:next;}
        }else if(frame<sample_frame||now<sample_stamp){period=0;cadence_count=cadence_slot=0;}
        sample_frame=frame;sample_stamp=now;
    }
    void reset(){active=false;vx=vy=0;stamp=0;count=0;}
    void start(double px,double py,int64_t now){active=true;x=base_x=target_x=px;y=base_y=target_y=py;vx=vy=0;stamp=now;count=0;clock=0;}
    static double positive(double v){return std::max(0.,v);}
    static double response(double t){
        if(t<=0)return 0;if(t>=span)return 1;
        const double a=positive(t-first),b=positive(t-second);
        return (t*t-a*a-b*b)/(2*first*second);
    }
    static double rate(double t){
        if(t<=0||t>=span)return 0;
        return (t-positive(t-first)-positive(t-second))/(first*second);
    }
    void advance(double tx,double ty,int64_t now,bool /*panning*/){
        if(!active)return;
        if(now<stamp||double(now-stamp)/1e9>.15||std::abs(tx-x)>32||std::abs(ty-y)>32){start(tx,ty,now);return;}
        const double scale=period>tick?tick/period:1.;
        clock+=double(now-stamp)/1e9*scale;
        unsigned done=0;
        while(done<count&&clock-steps[done].time>=span){base_x+=steps[done].dx;base_y+=steps[done].dy;++done;}
        if(done){for(unsigned i=done;i<count;++i)steps[i-done]=steps[i];count-=done;}
        if(tx!=target_x||ty!=target_y){
            if(count==steps.size()){start(tx,ty,now);return;}
            steps[count++]={clock,tx-target_x,ty-target_y};target_x=tx;target_y=ty;
        }
        x=base_x;y=base_y;vx=vy=0;
        for(unsigned i=0;i<count;++i){const double t=clock-steps[i].time,f=response(t),v=rate(t)*scale;
            x+=steps[i].dx*f;y+=steps[i].dy*f;vx+=steps[i].dx*v;vy+=steps[i].dy*v;}
        if(!count){x=target_x;y=target_y;}
        stamp=now;
    }
};
