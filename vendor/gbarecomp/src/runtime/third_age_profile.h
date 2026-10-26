#pragma once
#include "third_age_state.h"
#include <array>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
namespace gbarecomp {
// Opt-in bounded diagnostic. No guest writes, IO reads, input injection, or
// changes to cycle accounting. PC samples cover main-thread interpreter entries,
// not nested IRQ execution or native/fallback dispatch internals.
class ThirdAgeProfile {
    struct Bin { uint32_t pc=0,count=0; };
    struct Row {
        uint64_t frame=0,cycles=0,irqs=0,wall_ns=0,instructions=0,idle_cycles=0;
        uint64_t pc_samples=0,pc_dropped=0;
        uint32_t pc=0,cpsr=0; ThirdAgeState state;
        std::array<Bin,4> top{};
        uint64_t work_us=0,render_us=0,present_us=0,audio_us=0,input_us=0,wait_us=0,probe_us=0;
        bool presented=false,fast_forward=false,completed_image_clock=false;
        uint64_t ppu_frame=0,vblank=0,wait_before_present_us=0;
    };
    std::string path_;
    std::vector<Row> rows_;
    std::array<Bin,1024> bins_{};
    Row pending_;
    uint64_t total_=0,previous_exit_=0,instructions_=0,idle_cycles_=0,samples_=0,dropped_=0;
    uint32_t until_sample_=1021;
    bool sample_pc_=true;
    static uint64_t us(uint64_t a,uint64_t b){return b>=a?(b-a)/1000:0;}
public:
    static uint64_t now_ns(){return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
    ThirdAgeProfile(bool supported,bool interpreter) {
        const char* p=std::getenv("GBARECOMP_THIRD_AGE_PROFILE");
        if(!p||!*p)return;
        if(!supported||!interpreter){std::fprintf(stderr,"[third-age-profile] requires supported ROM and forced interpreter; disabled\n");return;}
        path_=p;rows_.resize(32768); // last ~9 minutes at normal speed, bounded memory
        if(const char* e=std::getenv("GBARECOMP_THIRD_AGE_PC_SAMPLES"))sample_pc_=e[0]!='0';
        std::fprintf(stderr,"[third-age-profile] v1 enabled; pc_samples=%d capacity=32768 -> %s\n",sample_pc_,path_.c_str());
    }
    bool active()const{return !path_.empty();}
    void instruction(uint32_t pc) {
        ++instructions_;
        if(!sample_pc_||--until_sample_)return;
        until_sample_=1021;++samples_;pc&=~255u;
        // Open addressing with counts as occupancy, allowing BIOS address zero.
        unsigned index=(pc>>8)*2654435761u % bins_.size();
        for(unsigned n=0;n<bins_.size();++n,index=(index+1)%bins_.size()) {
            auto& b=bins_[index];if(!b.count||b.pc==pc){b.pc=pc;++b.count;return;}
        }
        ++dropped_;
    }
    void idle(uint32_t cycles){idle_cycles_+=cycles;}
    void begin(uint64_t frame,uint64_t cycles,uint64_t irqs,uint32_t pc,uint32_t cpsr,const ThirdAgeState& state,bool presented,bool fast_forward,uint64_t t0,uint64_t ppu_frame=0,uint64_t vblank=0,bool completed_image_clock=false) {
        pending_=Row{};pending_.frame=frame;pending_.cycles=cycles;pending_.irqs=irqs;
        pending_.ppu_frame=ppu_frame;pending_.vblank=vblank;pending_.completed_image_clock=completed_image_clock;
        pending_.wall_ns=t0;pending_.pc=pc;pending_.cpsr=cpsr;pending_.state=state;
        pending_.presented=presented;pending_.fast_forward=fast_forward;
        pending_.instructions=instructions_;pending_.idle_cycles=idle_cycles_;
        pending_.pc_samples=samples_;pending_.pc_dropped=dropped_;
        for(auto& b:bins_) {
            if(b.count)for(unsigned j=0;j<pending_.top.size();++j)
                if(b.count>pending_.top[j].count) {
                    for(unsigned k=pending_.top.size()-1;k>j;--k)pending_.top[k]=pending_.top[k-1];
                    pending_.top[j]=b;break;
                }
            b.count=0;
        }
        instructions_=idle_cycles_=samples_=dropped_=0;
        // A varying prime stride avoids resetting to the same point per frame.
        // Sampling is deterministic and never uses the game's RNG.
        until_sample_=997+uint32_t(frame%47);
    }
    void finish(uint64_t t0,uint64_t captured,uint64_t t1,uint64_t t2,uint64_t t3,uint64_t t4,uint64_t t5,uint64_t early_wait_ns=0) {
        pending_.work_us=previous_exit_?us(previous_exit_,t0):0;
        pending_.probe_us=us(t0,captured);pending_.render_us=us(captured,t1);
        pending_.wait_before_present_us=early_wait_ns/1000;
        pending_.present_us=us(t1+early_wait_ns,t2);pending_.audio_us=us(t2,t3);
        pending_.input_us=us(t3,t4);pending_.wait_us=us(t4,t5)+early_wait_ns/1000;
        rows_[total_%rows_.size()]=pending_;++total_;previous_exit_=now_ns();
    }
    void dump()const {
        if(!active())return;
        std::FILE* f=std::fopen(path_.c_str(),"w");
        if(!f){std::fprintf(stderr,"[third-age-profile] cannot write %s\n",path_.c_str());return;}
        std::fprintf(f,"frame,cycles,irq_entries,wall_ns,main_instructions,idle_requested_cycles,pc_samples,pc_dropped,pc,cpsr,valid,scene,scene_arg,selection,substate,next_scene,player,side_raw,turn_mode_raw,setup_phase_raw,control_gate,ai_active,ai_enabled,action,camera_x,camera_y,cursor_x_fp,cursor_y_fp,cursor_address,selected_address,presented,fast_forward,work_devices_us,render_copy_us,present_us,audio_push_us,input_us,pacer_us,probe_us,pc0,count0,pc1,count1,pc2,count2,pc3,count3,ppu_frame,vblank,completed_image_clock,wait_before_present_us\n");
        const auto n=std::min<uint64_t>(total_,rows_.size());
        for(uint64_t i=0;i<n;++i) {
            const auto& r=rows_[(total_-n+i)%rows_.size()];const auto& s=r.state;
            std::fprintf(f,"%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u,%u,%d,%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
                (unsigned long long)r.frame,(unsigned long long)r.cycles,(unsigned long long)r.irqs,(unsigned long long)r.wall_ns,(unsigned long long)r.instructions,(unsigned long long)r.idle_cycles,(unsigned long long)r.pc_samples,(unsigned long long)r.pc_dropped,r.pc,r.cpsr,s.valid,s.scene,s.scene_arg,s.selection,s.substate,s.next_scene,s.player,s.side,s.turn_mode,s.setup_phase,s.control,s.ai_active,s.ai_enabled,s.action,s.camera_x,s.camera_y,s.cursor_x_fp,s.cursor_y_fp,s.cursor_address,s.selected_address,r.presented,r.fast_forward,(unsigned long long)r.work_us,(unsigned long long)r.render_us,(unsigned long long)r.present_us,(unsigned long long)r.audio_us,(unsigned long long)r.input_us,(unsigned long long)r.wait_us,(unsigned long long)r.probe_us);
            for(const auto& b:r.top)std::fprintf(f,",%u,%u",b.pc,b.count);
            std::fprintf(f,",%llu,%llu,%d,%llu\n",(unsigned long long)r.ppu_frame,(unsigned long long)r.vblank,r.completed_image_clock,(unsigned long long)r.wait_before_present_us);
        }
        const bool failed=std::ferror(f)!=0;const bool close_failed=std::fclose(f)!=0;
        std::fprintf(stderr,"[third-age-profile] %s rows=%llu total=%llu dropped_old_rows=%llu pc_stride=1021 buckets=256B top=4 -> %s\n",failed||close_failed?"WRITE FAILED":"saved",(unsigned long long)n,(unsigned long long)total_,(unsigned long long)(total_-n),path_.c_str());
    }
};
} // namespace gbarecomp
