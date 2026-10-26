#include <cstring>
#include <cmath>
#pragma once
#include "third_age_portrait_assets.h"
#include <unordered_map>
#ifdef THIRD_AGE_PORTABLE_RELEASE
#include "third_age_runtime_portraits.h"
#endif
namespace gbarecomp {
// Exact visible-pixel signatures from the verified 40x40 originals.
// No approximate identification; occluded or unknown art stays native; supported fades retain HD art.
#ifndef THIRD_AGE_PORTABLE_RELEASE
struct ThirdAgePortraitProbe {uint16_t offset;uint32_t rgb;};
// Private ROM-derived signatures are supplied locally; see docs/BUILD.md.
#include "third_age_portrait_patterns.private.inc"
#endif
struct ThirdAgeNativePortraitHit {int id,x,y,size=40;uint8_t red=255,green=255,blue=255;int anchor_x=0,anchor_y=0;};
class ThirdAgeNativePortraitMatcher {
    struct Probe {uint16_t offset;uint8_t index;};
    struct Pattern {int id,size,ax,ay;std::vector<Probe> probes;std::vector<uint32_t> palette;};
    std::vector<Pattern> patterns_;
    std::unordered_map<uint64_t,std::vector<int>> anchors_;
    // Each cached block owns anchor positions, not screen-space portraits.
    // Invalidate it when ANY pixel in a possible 40px portrait footprint
    // changes, so an unchanged anchor cannot hide a newly completed portrait.
    mutable std::vector<uint8_t> scan_previous_;
    mutable std::vector<std::vector<ThirdAgeNativePortraitHit>> scan_blocks_;
    mutable int scan_w_=0,scan_h_=0;
    mutable bool scan_valid_=false;
    struct DarkAnchor {int pattern,x,y,fade;};
    std::unordered_map<uint64_t,std::vector<DarkAnchor>> dark_;
    std::array<uint64_t,1024> signature_filter_{};
    // A canonical signature preserves equality between neighboring pixels.
    // Reject impossible edge masks before doing the more expensive palette
    // labeling. Masks are learned from every accepted fade signature, so this
    // cannot reject a match the full signature lookup would accept.
    std::array<uint64_t,64> edge_filter_{};
    template<class T> static unsigned edges(const T* p){
        return unsigned(p[0]==p[1])|unsigned(p[1]==p[2])<<1|
            unsigned(p[3]==p[4])<<2|unsigned(p[4]==p[5])<<3|
            unsigned(p[6]==p[7])<<4|unsigned(p[7]==p[8])<<5|
            unsigned(p[0]==p[3])<<6|unsigned(p[3]==p[6])<<7|
            unsigned(p[1]==p[4])<<8|unsigned(p[4]==p[7])<<9|
            unsigned(p[2]==p[5])<<10|unsigned(p[5]==p[8])<<11;
    }
    static unsigned filter_index(uint64_t key){return unsigned((key^(key>>16)^(key>>32))&65535);}

    static uint32_t dark_color(uint32_t rgb,int fade){
        uint32_t out=0;
        for(unsigned shift:{0u,8u,16u}){unsigned c=((rgb>>shift)&255)>>3;c=c*fade/16;out|=((c<<3)|(c>>2))<<shift;}
        return out;
    }
    template<class F> static uint64_t rgb_key(F color,int x,int y){
        uint64_t key=14695981039346656037ull;
        for(int j=0;j<3;++j)for(int i=0;i<3;++i){key^=color(x+i,y+j);key*=1099511628211ull;}
        return key;
    }
    template<class F> static uint64_t signature(F color,int x,int y){
        uint32_t seen[9];int count=0;uint64_t key=0;
        for(int j=0;j<3;++j)for(int i=0;i<3;++i){
            auto c=color(x+i,y+j);int k=0;while(k<count&&seen[k]!=c)++k;
            if(k==count)seen[count++]=c;
            key=(key<<4)|unsigned(k);
        }return count>=4?key:0;
    }
    void add(int id,int size,const std::vector<ThirdAgePortraitProbe>& source){
        Pattern p{id,size,0,0,{},{}};std::vector<uint32_t> grid(size*size,0xffffffffu);
        for(const auto& v:source)grid[v.offset]=v.rgb;
        const int margin=4; // Selection glow touches the rim, not the central face.
        int best=-1;uint64_t key=0;
        std::unordered_map<uint64_t,int> frequency;
        auto color=[&](int x,int y){return grid[y*size+x];};
        for(int y=margin;y<size-margin-2;++y)for(int x=margin;x<size-margin-2;++x)++frequency[signature(color,x,y)];
        for(int y=margin;y<size-margin-2;++y)for(int x=margin;x<size-margin-2;++x){
            std::vector<uint32_t> colors;bool valid=true;
            for(int j=0;j<3;++j)for(int i=0;i<3;++i){auto c=color(x+i,y+j);if(c==0xffffffffu)valid=false;else if(std::find(colors.begin(),colors.end(),c)==colors.end())colors.push_back(c);}
            std::vector<uint32_t> dark;
            for(auto c:colors){uint32_t d=0;for(unsigned shift:{0u,8u,16u})d|=((((c>>shift)&255)>>3)/4)<<shift;if(std::find(dark.begin(),dark.end(),d)==dark.end())dark.push_back(d);}
            const auto candidate=signature(color,x,y);const int score=(dark.size()>=4?100000:0)+10000/frequency[candidate]-int(colors.size());
            if(valid&&colors.size()>=4&&score>best){best=score;p.ax=x;p.ay=y;key=candidate;}
        }
        if(best<0)return;
        for(const auto& v:source){int x=v.offset%size,y=v.offset/size;if(x<margin||x>=size-margin||y<margin||y>=size-margin)continue;
            auto it=std::find(p.palette.begin(),p.palette.end(),v.rgb);int index=int(it-p.palette.begin());
            if(it==p.palette.end())p.palette.push_back(v.rgb);
            p.probes.push_back({v.offset,uint8_t(index)});
        }
        std::vector<uint64_t> keys{key};
        for(int fade=1;fade<=16;++fade){
            auto faded=[&](int x,int y){uint32_t rgb=color(x,y),out=0;for(unsigned shift:{0u,8u,16u}){unsigned c=((rgb>>shift)&255)>>3;c=c*fade/16;out|=((c<<3)|(c>>2))<<shift;}return out;};
            auto k=signature(faded,p.ax,p.ay);if(std::find(keys.begin(),keys.end(),k)==keys.end())keys.push_back(k);
        }
        // At 1/16..3/16 brightness, RGB5 quantization collapses the four
        // colors required by the general matcher. Use exact dark pixels and
        // verify the whole interior, with a separate non-flat anchor per fade.
        for(int fade=1;fade<=3;++fade){
            auto faded=[&](int x,int y){return dark_color(color(x,y),fade);};
            std::unordered_map<uint64_t,int> counts;
            for(int y=margin;y<size-margin-2;++y)for(int x=margin;x<size-margin-2;++x)++counts[rgb_key(faded,x,y)];
            int score=-1,ax=0,ay=0;uint64_t selected=0;
            for(int y=margin;y<size-margin-2;++y)for(int x=margin;x<size-margin-2;++x){
                std::vector<uint32_t> distinct;bool valid=true;
                for(int j=0;j<3;++j)for(int i=0;i<3;++i){
                    if(color(x+i,y+j)==0xffffffffu)valid=false;
                    auto c=faded(x+i,y+j);if(std::find(distinct.begin(),distinct.end(),c)==distinct.end())distinct.push_back(c);
                }
                const auto k=rgb_key(faded,x,y);int value=10000/counts[k]+int(distinct.size());
                if(valid&&distinct.size()>=2&&value>score){score=value;ax=x;ay=y;selected=k;}
            }
            if(score>=0)dark_[selected].push_back({int(patterns_.size()),ax,ay,fade});
        }
        for(auto k:keys)if(k){
            anchors_[k].push_back(int(patterns_.size()));
            unsigned labels[9];for(unsigned i=0;i<9;++i)labels[i]=(k>>(4*(8-i)))&15;
            const auto e=edges(labels);edge_filter_[e/64]|=uint64_t(1)<<(e%64);
            const auto bit=filter_index(k);signature_filter_[bit/64]|=uint64_t(1)<<(bit%64);
        }
        patterns_.push_back(std::move(p));
    }
public:
    ThirdAgeNativePortraitMatcher(){for(int i=0;i<68;++i){add(i,40,third_age_portrait_patterns()[i]);add(i,32,third_age_small_portrait_patterns()[i]);}}
    std::vector<ThirdAgeNativePortraitHit> scan_region(const uint8_t* rgb,int w,int h,int x0,int y0,int x1,int y1)const{
        std::vector<ThirdAgeNativePortraitHit> hits;
        if(third_age_original_portraits()||!rgb||w<32||h<32||w>320||h>180)return hits;
        auto color=[&](int x,int y){const auto* p=rgb+(y*w+x)*3;return (uint32_t(p[0])<<16)|(uint32_t(p[1])<<8)|p[2];};
        auto record=[&](ThirdAgeNativePortraitHit hit,int anchor_x,int anchor_y){
            hit.anchor_x=anchor_x;hit.anchor_y=anchor_y;
            for(const auto& old:hits)if(old.x==hit.x&&old.y==hit.y&&old.size==hit.size)return;
            hits.push_back(hit);
        };
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
            // 1/16..3/16 fades contain RGB5 channels at most 5 (expanded 41).
            // A brighter first pixel cannot match any exact dark anchor.
            const auto first=color(x,y);
            const bool possibly_dark=((first>>16)&255)<=41&&((first>>8)&255)<=41&&(first&255)<=41;
            auto dark=possibly_dark?dark_.find(rgb_key(color,x,y)):dark_.end();
            if(dark!=dark_.end())for(const auto& a:dark->second){
                const auto& p=patterns_[a.pattern];const int left=x-a.x,top=y-a.y;
                if(left<0||top<0||left+p.size>w||top+p.size>h)continue;
                bool match=true;
                for(const auto& v:p.probes)
                    if(color(left+v.offset%p.size,top+v.offset/p.size)!=dark_color(p.palette[v.index],a.fade)){match=false;break;}
                if(match){const auto mod=uint8_t((a.fade*255+8)/16);record({p.id,left,top,p.size,mod,mod,mod},x,y);if(hits.size()>=12)return hits;}
            }
            uint32_t patch[9];
            for(int j=0;j<3;++j)for(int i=0;i<3;++i)patch[j*3+i]=color(x+i,y+j);
            const auto edge=edges(patch);
            if(!(edge_filter_[edge/64]&(uint64_t(1)<<(edge%64))))continue;
            const auto key=signature([&](int px,int py){return patch[py*3+px];},0,0);const auto bit=filter_index(key);
            if(!(signature_filter_[bit/64]&(uint64_t(1)<<(bit%64))))continue;
            auto it=anchors_.find(key);if(it==anchors_.end())continue;
            for(int candidate:it->second){const auto& p=patterns_[candidate];int left=x-p.ax,top=y-p.ay;
                if(left<0||top<0||left+p.size>w||top+p.size>h)continue;
                // Match the entire interior by palette relationships. This survives
                // fades without approximate face matching or retaining stale hits.
                std::array<uint32_t,16> mapped;mapped.fill(0xffffffffu);bool match=true;
                for(const auto& v:p.probes){auto c=color(left+v.offset%p.size,top+v.offset/p.size);auto& old=mapped[v.index];if(old==0xffffffffu)old=c;else if(old!=c){match=false;break;}}
                if(!match)continue;
                auto unique=mapped;std::sort(unique.begin(),unique.begin()+p.palette.size());
                if(std::unique(unique.begin(),unique.begin()+p.palette.size())-unique.begin()<4)continue;
                // Reject flat or unrelated palette substitutions; fit a fade to black.
                uint8_t modulation[3];
                for(int ch=0;ch<3&&match;++ch){int shift=16-8*ch;double aa=0,ab=0;
                    for(size_t k=0;k<p.palette.size();++k){double a=(p.palette[k]>>shift)&255,b=(mapped[k]>>shift)&255;aa+=a*a;ab+=a*b;}
                    double gain=aa?ab/aa:1;gain=std::clamp(gain,0.,1.);
                    double error=0;for(size_t k=0;k<p.palette.size();++k){double a=(p.palette[k]>>shift)&255,b=(mapped[k]>>shift)&255;error+=std::abs(a*gain-b);}
                    if(error/p.palette.size()>6)match=false;
                    modulation[ch]=uint8_t(std::lround(gain*255));
                }
                if(!match||std::max({modulation[0],modulation[1],modulation[2]})<12)continue;
                record({p.id,left,top,p.size,modulation[0],modulation[1],modulation[2]},x,y);if(hits.size()>=12)return hits;
            }
        }return hits;
    }
    std::vector<ThirdAgeNativePortraitHit> scan(const uint8_t* rgb,int w,int h)const{
        if(third_age_original_portraits()||!rgb||w<32||h<32||w>320||h>180)return {};
        constexpr int block=8,reach=40;
        const int cols=(w+block-1)/block,rows=(h+block-1)/block,stride=cols+1;
        const size_t bytes=size_t(w)*h*3;
        const bool reset=w!=scan_w_||h!=scan_h_||scan_previous_.size()!=bytes;
        if(reset){scan_blocks_.assign(size_t(cols)*rows,{});scan_w_=w;scan_h_=h;scan_valid_=false;}
        std::vector<unsigned> changes(size_t(cols+1)*(rows+1),0);
        for(int ty=0;ty<rows;++ty)for(int tx=0;tx<cols;++tx){
            bool dirty=reset;
            for(int y=ty*block;!dirty&&y<std::min(h,(ty+1)*block);++y){
                size_t offset=(size_t(y)*w+tx*block)*3;
                dirty=std::memcmp(rgb+offset,scan_previous_.data()+offset,std::min(block,w-tx*block)*3)!=0;
            }
            const int at=(ty+1)*stride+tx+1;
            changes[at]=unsigned(dirty)+changes[at-1]+changes[at-stride]-changes[at-stride-1];
        }
        // Full-screen motion/fades cannot reuse regions. Keep the original
        // scan path there, and rebuild the block cache once motion settles.
        if(!reset&&changes.back()>unsigned(cols*rows/2)){
            scan_valid_=false;scan_previous_.assign(rgb,rgb+bytes);
            return scan_region(rgb,w,h,0,0,w-2,h-2);
        }
        std::vector<ThirdAgeNativePortraitHit> candidates;
        for(int ty=0;ty<rows;++ty)for(int tx=0;tx<cols;++tx){
            const int x0=tx*block,y0=ty*block,x1=std::min(w-2,x0+block),y1=std::min(h-2,y0+block);
            if(x0>=x1||y0>=y1)continue;
            const int lx=std::max(0,x0-reach)/block,ly=std::max(0,y0-reach)/block;
            const int rx=(std::min(w,x1+reach)+block-1)/block,ry=(std::min(h,y1+reach)+block-1)/block;
            const unsigned changed=changes[ry*stride+rx]+changes[ly*stride+lx]-changes[ry*stride+lx]-changes[ly*stride+rx];
            auto& hits=scan_blocks_[ty*cols+tx];
            if(changed||!scan_valid_)hits=scan_region(rgb,w,h,x0,y0,x1,y1);
            candidates.insert(candidates.end(),hits.begin(),hits.end());
        }
        scan_previous_.assign(rgb,rgb+bytes);scan_valid_=true;
        // Restore original scan order and de-duplicate anchors in other blocks.
        // This also retains the original fade choice and global 12-hit bound.
        std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){
            return a.anchor_y!=b.anchor_y?a.anchor_y<b.anchor_y:a.anchor_x<b.anchor_x;
        });
        std::vector<ThirdAgeNativePortraitHit> hits;
        for(const auto& hit:candidates){
            bool duplicate=false;
            for(const auto& old:hits)if(old.x==hit.x&&old.y==hit.y&&old.size==hit.size){duplicate=true;break;}
            if(!duplicate)hits.push_back(hit);
            if(hits.size()>=12)break;
        }
        return hits;
    }
};
class ThirdAgeNativePortraits {
    ThirdAgeNativePortraitMatcher matcher_;
    ThirdAgePortraitAssets assets_;
    struct Texture {SDL_Texture* texture=nullptr;uint64_t age=0;};
    std::map<int,Texture> textures_;uint64_t clock_=0;
    std::vector<ThirdAgeNativePortraitHit> hits_;
    std::vector<uint8_t> previous_,unblended_;int previous_w_=0,previous_h_=0;
    std::vector<ThirdAgeNativePortraitHit> previous_hits_;
public:
    ThirdAgeNativePortraits():assets_([](){char* path=SDL_GetBasePath();std::filesystem::path p=path?path:"";SDL_free(path);return p;}()){}
    void observe(const uint8_t* rgb,int w,int h,bool enabled){
        if(!enabled||!rgb||w<32||h<32||w>320||h>180){hits_.clear();previous_hits_.clear();previous_.clear();return;}
        const size_t bytes=size_t(w)*h*3;
        if(w==previous_w_&&h==previous_h_&&previous_.size()==bytes&&std::equal(previous_.begin(),previous_.end(),rgb))return;
        previous_hits_=(w==previous_w_&&h==previous_h_)?hits_:std::vector<ThirdAgeNativePortraitHit>{};
        hits_=matcher_.scan(rgb,w,h);previous_.assign(rgb,rgb+bytes);previous_w_=w;previous_h_=h;
    }
    const uint8_t* prepare_frame(const uint8_t* raw,const uint8_t* mixed,int w,int h){
        if(!raw||!mixed||raw==mixed||w!=previous_w_||h!=previous_h_||
           (hits_.empty()&&previous_hits_.empty()))return mixed;
        unblended_.assign(mixed,mixed+size_t(w)*h*3);
        // Restore current native pixels in portrait slots before drawing HD.
        // Also clear the old slot when a hero moves, changes size or disappears.
        // Elsewhere, including battlefield dithering, frame blending is intact.
        auto restore=[&](const auto& list){for(const auto& hit:list){
            if(hit.x<0||hit.y<0||hit.x+hit.size>w||hit.y+hit.size>h)continue;
            for(int y=hit.y;y<hit.y+hit.size;++y){size_t offset=(size_t(y)*w+hit.x)*3;
                std::copy_n(raw+offset,hit.size*3,unblended_.data()+offset);
            }
        }};
        restore(previous_hits_);restore(hits_);return unblended_.data();
    }
    void release(){for(auto& item:textures_)SDL_DestroyTexture(item.second.texture);textures_.clear();hits_.clear();previous_hits_.clear();previous_.clear();unblended_.clear();}
    void draw(SDL_Renderer* r,const SDL_Rect& game,int w,int h){
        if(w<=0||h<=0)return;
        int lw=0,lh=0;SDL_RenderGetLogicalSize(r,&lw,&lh);
        const SDL_Rect area=lw&&lh?SDL_Rect{0,0,lw,lh}:game;
        for(const auto& hit:hits_){
            auto it=textures_.find(hit.id);
            if(it==textures_.end()){
                auto* art=assets_.get(hit.id);if(!art)continue;
                auto* t=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,art->size,art->size);if(!t)continue;
                if(SDL_UpdateTexture(t,nullptr,art->pixels.data(),art->size*4)!=0){SDL_DestroyTexture(t);continue;}
                SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2,0,12)
                SDL_SetTextureScaleMode(t,SDL_ScaleModeLinear);
#endif
                if(textures_.size()>=8){auto victim=std::min_element(textures_.begin(),textures_.end(),[](const auto& a,const auto& b){return a.second.age<b.second.age;});SDL_DestroyTexture(victim->second.texture);textures_.erase(victim);}
                it=textures_.emplace(hit.id,Texture{t,0}).first;
            }
            it->second.age=++clock_;
            const SDL_FRect dst{float(area.x+double(hit.x)*area.w/w),float(area.y+double(hit.y)*area.h/h),float(double(hit.size)*area.w/w),float(double(hit.size)*area.h/h)};
            SDL_SetTextureColorMod(it->second.texture,hit.red,hit.green,hit.blue);
            SDL_RenderCopyF(r,it->second.texture,nullptr,&dst);
        }
    }
};
}
