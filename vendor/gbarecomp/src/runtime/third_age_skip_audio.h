#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <vector>

namespace gbarecomp {
// Host-only handoff for manually accelerated combat. Feed chronological audio
// at elapsed wall time, not at the guest's accelerated rate. A bounded backlog
// is discarded on return and the live stream is blended in over 31 milliseconds.
// This smooths the cut; it cannot preserve the original music's beat position.
class ThirdAgeSkipAudio {
    static constexpr uint64_t rate=65536,fade_samples=2048,max_pending=131072;
    std::deque<int16_t> pending_,tail_;
    std::vector<int16_t> fade_;
    uint64_t start_=0,sent_=0,paused_at_=0;
    std::size_t fade_pos_=0;
    bool active_=false,paused_=false;
public:
    std::vector<int16_t> output;
    bool active()const{return active_;}
    bool filtering()const{return active_||fade_pos_<fade_.size();}
    std::size_t pending()const{return pending_.size();}
    uint64_t sent()const{return sent_;}
    void clear(){pending_.clear();tail_.clear();fade_.clear();output.clear();start_=sent_=paused_at_=fade_pos_=0;active_=paused_=false;}
    void remember(const int16_t* samples,std::size_t count){
        if(count>=fade_samples){tail_.clear();samples+=count-fade_samples;count=fade_samples;}
        for(std::size_t i=0;i<count;++i)tail_.push_back(samples[i]);
        while(tail_.size()>fade_samples)tail_.pop_front();
    }
    void begin(uint64_t now){active_=true;paused_=false;start_=now;sent_=0;pending_.clear();fade_.clear();fade_pos_=0;}
    void pause(bool paused,uint64_t now){
        if(!active_||paused==paused_)return;
        if(paused)paused_at_=now;
        else if(now>=paused_at_)start_+=now-paused_at_;
        paused_=paused;
    }
    void end(){
        if(!active_)return;
        active_=paused_=false;pending_.clear();
        // Start at the last submitted sample to avoid an amplitude step at
        // the join; a short reversed tail fades out as live audio fades in.
        fade_.assign(tail_.rbegin(),tail_.rend());fade_pos_=0;
    }
    void process(const int16_t* samples,std::size_t count,uint64_t now){
        output.clear();
        if(active_){
            // Keep the chronological beginning of a fast-produced segment;
            // do not flood the output ring or queue seconds of delayed SFX.
            for(std::size_t i=0;i<count&&pending_.size()<max_pending;++i)pending_.push_back(samples[i]);
            if(paused_)return;
            const uint64_t elapsed=now>=start_?now-start_:0;
            const uint64_t target=rate*(elapsed/1000000000ull)+rate*(elapsed%1000000000ull)/1000000000ull+2048;
            const auto n=std::min<uint64_t>({target>sent_?target-sent_:0,pending_.size(),4096});
            output.reserve(n);
            for(uint64_t i=0;i<n;++i){output.push_back(pending_.front());pending_.pop_front();}
            sent_+=n;
        }else{
            output.assign(samples,samples+count);
            for(std::size_t i=0;i<count&&fade_pos_<fade_.size();++i,++fade_pos_){
                const int64_t n=int64_t(fade_.size()),w=int64_t(fade_pos_)+1;
                output[i]=int16_t((int64_t(fade_[fade_pos_])*(n-w)+int64_t(output[i])*w)/n);
            }
        }
    }
};
}
