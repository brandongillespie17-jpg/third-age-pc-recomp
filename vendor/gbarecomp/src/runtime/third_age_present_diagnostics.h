#pragma once
#include "third_age_refresh_clock.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <locale>
#include <string>
#include <vector>
namespace gbarecomp {
// Separate from the legacy per-present CSV. No file operations or allocation in
// record(); only the enabled diagnostic allocates a bounded ring at window open.
#define THIRD_AGE_DIAGNOSTIC_FIELDS(X) \
    X(uint64_t, sequence) \
    X(uint64_t, source_sequence) \
    X(uint64_t, guest_frame) \
    X(uint64_t, base_source_frame) \
    X(uint64_t, camera_source_frame) \
    X(int64_t, deadline_ns) \
    X(int64_t, period_ns) \
    X(int64_t, refresh_start_ns) \
    X(int64_t, refresh_end_ns) \
    X(int64_t, present_entry_ns) \
    X(int64_t, present_exit_ns) \
    X(int64_t, portrait_observe_end_ns) \
    X(int64_t, base_upload_entry_ns) \
    X(int64_t, base_upload_exit_ns) \
    X(int64_t, camera_upload_entry_ns) \
    X(int64_t, camera_upload_exit_ns) \
    X(int64_t, preparation_end_ns) \
    X(int64_t, source_completed_ns) \
    X(int64_t, source_complete_age_ns) \
    X(int64_t, source_received_ns) \
    X(int64_t, base_uploaded_ns) \
    X(int64_t, camera_uploaded_ns) \
    X(int64_t, source_age_ns) \
    X(int64_t, base_upload_age_ns) \
    X(int64_t, camera_upload_age_ns) \
    X(int64_t, lateness_ns) \
    X(uint64_t, missed_periods) \
    X(uint64_t, resync_count) \
    X(unsigned, origin) \
    X(unsigned, clock_action) \
    X(unsigned, base_upload_error) \
    X(unsigned, camera_blocked) \
    X(unsigned, camera_available) \
    X(unsigned, camera_drawn) \
    X(unsigned, camera_upload_errors) \
    X(unsigned, camera_draw_errors) \
    X(unsigned, camera_reset_flags) \
    X(unsigned, camera_tween_reason) \
    X(unsigned, unit_motion_reason) \
    X(unsigned, unit_tween_reason) \
    X(unsigned, unit_layer) \
    X(unsigned, weather_layer) \
    X(unsigned, continuous_scroll) \
    X(unsigned, history_depth) \
    X(unsigned, mouse_owned) \
    X(unsigned, edge_panning) \
    X(int, camera_sim_x) \
    X(int, camera_sim_y) \
    X(int, camera_target_x) \
    X(int, camera_target_y) \
    X(int, unit_target_x) \
    X(int, unit_target_y) \
    X(int, layout_width) \
    X(int, layout_height) \
    X(float, renderer_scale_x) \
    X(float, renderer_scale_y) \
    X(double, camera_raw_dx) \
    X(double, camera_raw_dy) \
    X(double, camera_snapped_dx) \
    X(double, camera_snapped_dy) \
    X(double, camera_displayed_x) \
    X(double, camera_displayed_y) \
    X(double, unit_raw_dx) \
    X(double, unit_raw_dy) \
    X(double, unit_snapped_dx) \
    X(double, unit_snapped_dy) \
    X(double, unit_displayed_x) \
    X(double, unit_displayed_y) \
    X(double, unit_total_dx) \
    X(double, unit_total_dy) \
    X(double, camera_alpha) \
    X(double, camera_duration_ns) \
    X(double, unit_alpha) \
    X(double, unit_duration_ns) \
    X(double, scroll_vx) \
    X(double, scroll_vy) \
    X(double, scroll_period_ns) \
    X(unsigned, scroll_queue_depth)
struct ThirdAgePresentSample {
#define FIELD(type, name) type name = 0;
    THIRD_AGE_DIAGNOSTIC_FIELDS(FIELD)
#undef FIELD
};
class ThirdAgePresentDiagnostics {
    std::string path_;
    std::vector<ThirdAgePresentSample> rows_;
    uint64_t total_=0,resyncs_=0;
public:
    static constexpr size_t capacity=65536;
    ThirdAgePresentDiagnostics(){
        const char* path=std::getenv("THIRD_AGE_DIAGNOSTIC_TRACE");
        if(!path||!*path)return;
        path_=path;rows_.resize(capacity);
        std::fprintf(stderr,"[third-age-diagnostic] v1 enabled capacity=%zu bytes=%zu -> %s\n",capacity,rows_.size()*sizeof(ThirdAgePresentSample),path_.c_str());
    }
    bool active()const{return !rows_.empty();}
    static void clock_sample(ThirdAgePresentSample& s,const ThirdAgeRefreshClock& clock,int64_t start){
        s.refresh_start_ns=start;s.period_ns=clock.period_ns();s.deadline_ns=clock.deadline_ns();
        // 0 disabled, 1 initial anchor, 2 normal advance, 3 late resync.
        s.clock_action=!s.period_ns?0:!s.deadline_ns?1:start-s.deadline_ns>s.period_ns?3:2;
        if(s.deadline_ns){s.lateness_ns=std::max(int64_t(0),start-s.deadline_ns);
            if(s.period_ns)s.missed_periods=s.lateness_ns/s.period_ns;}
    }
    void record(ThirdAgePresentSample sample){
        if(!active())return;
        sample.sequence=++total_;if(sample.clock_action==3)++resyncs_;sample.resync_count=resyncs_;
        rows_[(total_-1)%rows_.size()]=sample;
    }
    void dump()const{
        if(!active())return;
        std::ofstream out(path_);out.imbue(std::locale::classic());out<<std::setprecision(12);
        if(!out){std::fprintf(stderr,"[third-age-diagnostic] cannot write %s\n",path_.c_str());return;}
        bool first=true;
#define HEADER(type, name) if(!first)out<<',';first=false;out<<#name;
        THIRD_AGE_DIAGNOSTIC_FIELDS(HEADER)
#undef HEADER
        out<<'\n';const uint64_t n=std::min<uint64_t>(total_,rows_.size());
        for(uint64_t i=0;i<n;++i){const auto& row=rows_[(total_-n+i)%rows_.size()];first=true;
#define VALUE(type, name) if(!first)out<<',';first=false;out<<row.name;
            THIRD_AGE_DIAGNOSTIC_FIELDS(VALUE)
#undef VALUE
            out<<'\n';
        }
        out.close();
        std::fprintf(stderr,"[third-age-diagnostic] %s rows=%llu total=%llu dropped_old_rows=%llu -> %s\n",out.fail()?"WRITE FAILED":"saved",(unsigned long long)n,(unsigned long long)total_,(unsigned long long)(total_-n),path_.c_str());
    }
};
#undef THIRD_AGE_DIAGNOSTIC_FIELDS
} // namespace gbarecomp
