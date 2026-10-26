#pragma once
// Bounded, opt-in, read-only rendering evidence. No guest writes or timing calls.
// Recording is explicitly diagnostic, not an FPS unlock or a benchmark mode.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>
#include <vector>

namespace gbarecomp {
inline const std::string& third_age_capture_directory() {
    static const std::string path=[] {const char* p=std::getenv("THIRD_AGE_VIDEO_CAPTURE");return std::string(p?p:"");}();
    return path;
}
inline std::atomic<bool> third_age_capture_requested{false};
inline bool third_age_request_capture() {
    if(third_age_capture_directory().empty())return false;
    third_age_capture_requested.store(true);return true;
}

class ThirdAgeVideoCapture {
    struct Limit {};
    static constexpr size_t max_bytes=64u*1024u*1024u;
    static constexpr unsigned max_frames=180,max_clips=8;
    std::vector<uint8_t> bytes_;
    std::array<std::vector<uint8_t>,9> previous_;
    size_t complete_=0;
    unsigned clips_=0,frames_=0,line_=0;
    bool active_=false,failed_=false;
    uint64_t epoch_=0;
    static void u32(std::vector<uint8_t>& out,uint32_t v) {
        for(unsigned i=0;i<4;++i)out.push_back(uint8_t(v>>(8*i)));
    }
    static void u64(std::vector<uint8_t>& out,uint64_t v) {u32(out,uint32_t(v));u32(out,uint32_t(v>>32));}
    static uint64_t now() {return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
    static uint64_t hash(const uint8_t* p,size_t n) {
        // Adler-32: independently checked by Python's zlib on collection.
        uint32_t a=1,b=0;
        while(n){const size_t part=std::min(n,size_t(5552));
            for(size_t i=0;i<part;++i){a+=p[i];b+=a;}a%=65521;b%=65521;p+=part;n-=part;
        }return (b<<16)|a;
    }
    void record(unsigned tag,unsigned y,const uint8_t* p,size_t n) {
        if(n>max_bytes||bytes_.size()+12+n+32>max_bytes)throw Limit{};
        u32(bytes_,tag);u32(bytes_,y);u32(bytes_,uint32_t(n));
        if(n)bytes_.insert(bytes_.end(),p,p+n);
    }
    void words(unsigned tag,unsigned y,std::initializer_list<uint64_t> values) {
        std::vector<uint8_t> out;out.reserve(8*values.size());
        for(auto v:values)u64(out,v);
        record(tag,y,out.data(),out.size());
    }
    // Each region starts with a full image, including all-zero bytes. Later
    // records carry changed 256-byte blocks. Raster IO is limited to video IO.
    void delta(unsigned tag,unsigned y,const uint8_t* p,size_t n) {
        auto& old=previous_.at(tag-10);
        if(old.size()!=n){
            std::vector<uint8_t> out;out.reserve(n+4);u32(out,0);out.insert(out.end(),p,p+n);
            record(tag,y,out.data(),out.size());old.assign(p,p+n);return;
        }
        std::array<uint8_t,260> out{};
        for(size_t off=0;off<n;off+=256){
            const size_t count=std::min(size_t(256),n-off);
            if(std::memcmp(p+off,old.data()+off,count)==0)continue;
            for(unsigned i=0;i<4;++i)out[i]=uint8_t(off>>(8*i));
            std::memcpy(out.data()+4,p+off,count);record(tag,y,out.data(),count+4);
            std::memcpy(old.data()+off,p+off,count);
        }
    }
    void finish(unsigned reason) noexcept {
        if(!active_)return;
        active_=false;
        try {
            // Never publish an incomplete raster frame, even on a memory cap,
            // restore, exit, or failed allocation partway through a scanline.
            bytes_.resize(complete_);
            if(frames_){
                // Ordinary records reserve 32 bytes for this 28-byte footer.
                u32(bytes_,255);u32(bytes_,0);u32(bytes_,16);
                u64(bytes_,frames_);u64(bytes_,reason);
                const auto dir=std::filesystem::path(third_age_capture_directory());
                if(!std::filesystem::is_directory(dir))throw std::runtime_error("capture directory missing");
                const auto target=dir/("clip-"+std::to_string(clips_)+".tavcap");
                const auto temporary=dir/("clip-"+std::to_string(clips_)+".partial");
                if(std::filesystem::exists(target)||std::filesystem::exists(temporary))throw std::runtime_error("capture file already exists");
                std::ofstream out(temporary,std::ios::binary);
                out.write(reinterpret_cast<const char*>(bytes_.data()),std::streamsize(bytes_.size()));out.close();
                if(!out)throw std::runtime_error("capture write failed");
                std::filesystem::rename(temporary,target);
                std::fprintf(stderr,"third-age-capture: saved %u complete frames to %s (reason %u)\n",frames_,target.string().c_str(),reason);
            }
        } catch(const std::exception& e) {failed_=true;std::fprintf(stderr,"third-age-capture: stopped: %s\n",e.what());}
          catch(...) {failed_=true;std::fprintf(stderr,"third-age-capture: stopped while saving\n");}
        bytes_.clear();for(auto& p:previous_)p.clear();complete_=0;frames_=line_=0;
    }
public:
    ~ThirdAgeVideoCapture(){finish(3);}
    static bool enabled(){return !third_age_capture_directory().empty();}
    void restored() noexcept {finish(4);++epoch_;third_age_capture_requested.store(false);}
    bool active()const{return active_;}
    // Called BEFORE the expanded renderer adjusts any scratch video state.
    void scanline(uint64_t frame,unsigned y,unsigned display,const uint8_t* iw,const uint8_t* ew,
                  const uint8_t* io,const uint8_t* vr,const uint8_t* oam,const uint8_t* pal,
                  const std::array<int32_t,12>& affine) noexcept {
        if(third_age_capture_directory().empty()||failed_)return;
        try {
            if(y==0&&third_age_capture_requested.exchange(false)) {
                if(active_){finish(1);return;}
                if(clips_>=max_clips){std::fprintf(stderr,"third-age-capture: eight-clip limit reached; quit to package\n");return;}
                ++clips_;bytes_.assign({'T','A','V','C','A','P','0','1'});complete_=8;frames_=line_=0;
                for(auto& p:previous_)p.clear();
                active_=true;
                std::fprintf(stderr,"third-age-capture: recording clip %u for up to 3 seconds; F11 again stops early\n",clips_);
            }
            if(!active_)return;
            if(y!=line_||y>=160||!iw||!ew){finish(5);return;}
            if(y==0){words(1,0,{frame,epoch_,now()});delta(10,0,iw,32768);delta(11,0,ew,262144);}
            delta(12,y,vr,98304);delta(13,y,oam,1024);delta(14,y,pal,1024);delta(15,y,io,96);
            std::vector<uint8_t> a;u32(a,display);for(auto v:affine)u32(a,uint32_t(v));
            record(3,y,a.data(),a.size());++line_;
        } catch(const Limit&) {finish(2);}
          catch(...) {finish(6);failed_=true;}
    }
    void frame_ready(const uint8_t* native,const uint8_t* expanded,uint64_t key,
                     int left,int top,int cx,int cy,uint32_t settings) noexcept {
        if(!active_)return;
        try {
            if(line_!=160||!native){finish(5);return;}
            delta(17,160,native,240*160*3);
            if(expanded)delta(18,160,expanded,320*180*3);
            words(4,160,{now(),hash(native,240*160*3),expanded?hash(expanded,320*180*3):0,
                        key,uint32_t(left),uint32_t(top),uint32_t(cx),uint32_t(cy),settings});
            ++frames_;line_=0;complete_=bytes_.size();
            if(frames_>=max_frames)finish(0);
        } catch(const Limit&) {finish(2);}
          catch(...) {finish(6);failed_=true;}
    }
};
}
