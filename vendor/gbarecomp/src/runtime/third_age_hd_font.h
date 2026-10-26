#pragma once
// Ringbearer is loaded from its unchanged, embedded TTF. This small reader
// builds only the runtime outline cache used by the host rasterizer; no font
// file is edited or subsetted. Complete original archive: assets/fonts/ringbearer.zip.
// Copyright Pete Klassen, 2002. Private use; see the original readme for terms.
#ifndef THIRD_AGE_PORTABLE_RELEASE
#include "third_age_ringbearer_data.h"
#endif
#include <filesystem>
#include "utf8_path.h"
#include <fstream>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <array>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace gbarecomp::hd_font {
struct Edge {int16_t x0,y0,x1,y1;};
struct Glyph {uint32_t first,count;int advance;};
class Font {
    std::vector<uint8_t> data_;
    unsigned work_=0;
    void work(unsigned n=1){if(n>2000000-work_)throw std::runtime_error("Font work limit");work_+=n;}
    struct Point {double x,y;bool on;};
    struct Transform {double a=1,b=0,c=0,d=1,x=0,y=0;};
    uint32_t head=0,loca=0,glyf=0,cmap=0,hmtx=0,hhea=0;
    std::array<unsigned,96> ids{};
    std::unordered_map<uint32_t,int> pairs;
    unsigned u8(size_t p)const{if(p>=data_.size())throw std::runtime_error("Font bounds");return data_[p];}
    unsigned u16(size_t p)const{return u8(p)*256+u8(p+1);}
    int i16(size_t p)const{return int16_t(u16(p));}
    uint32_t u32(size_t p)const{return (uint32_t(u16(p))<<16)|u16(p+2);}
    uint32_t table(uint32_t tag)const{for(unsigned j=0;j<u16(4);++j){size_t p=12+16*j;if(u32(p)==tag)return u32(p+8);}return 0;}
    unsigned character(unsigned ch)const{
        // Windows/Unicode BMP cmap format 4, present in the supplied TTF.
        unsigned count=u16(cmap+6)/2;size_t ends=cmap+14,starts=ends+2*count+2,deltas=starts+2*count,ranges=deltas+2*count;
        for(unsigned j=0;j<count;++j)if(ch>=u16(starts+2*j)&&ch<=u16(ends+2*j)){
            unsigned distance=u16(ranges+2*j),id=distance?u16(ranges+2*j+distance+2*(ch-u16(starts+2*j))):ch;
            return id?(id+i16(deltas+2*j))&65535:0;
        }return 0;
    }
    Point transformed(Point p,const Transform& t)const{return {t.a*p.x+t.c*p.y+t.x,t.b*p.x+t.d*p.y+t.y,p.on};}
    void line(Point a,Point b,const Transform& t){work();a=transformed(a,t);b=transformed(b,t);
        for(double v:{a.x,a.y,b.x,b.y})if(!std::isfinite(v)||v < -32768||v > 32767)throw std::runtime_error("Font coordinate bounds");if(a.x==b.x&&a.y==b.y)return;
        edges.push_back({int16_t(std::lround(a.x)),int16_t(std::lround(a.y)),int16_t(std::lround(b.x)),int16_t(std::lround(b.y))});}
    void curve(Point a,Point b,Point c,const Transform& t){Point prev=a;
        // At most two font units of chord error, well below a display pixel.
        // Small curves need far fewer segments than a fixed 16-step split.
        const int steps=std::clamp(int(std::ceil(std::sqrt(std::hypot(a.x-2*b.x+c.x,a.y-2*b.y+c.y)/8.))),1,32);
        for(int j=1;j<=steps;++j){double k=double(j)/steps,q=1-k;Point p{q*q*a.x+2*q*k*b.x+k*k*c.x,q*q*a.y+2*q*k*b.y+k*k*c.y,true};line(prev,p,t);prev=p;}}
    void contour(const std::vector<Point>& all,unsigned first,unsigned last,const Transform& t){
        if(last<first||last>=all.size())throw std::runtime_error("Ringbearer contour");
        Point start=all[first];unsigned at=first;
        if(start.on)++at;else if(all[last].on){start=all[last];--last;}else start={(start.x+all[last].x)/2,(start.y+all[last].y)/2,true};
        Point current=start;
        while(at<=last){auto p=all[at++];if(p.on){line(current,p,t);current=p;continue;}
            Point next=at<=last?all[at]:start;
            if(next.on){curve(current,p,next,t);current=next;++at;}
            else{Point mid{(p.x+next.x)/2,(p.y+next.y)/2,true};curve(current,p,mid,t);current=mid;}}
        line(current,start,t);
    }
    void outline(unsigned id,const Transform& t,unsigned depth=0){
        work();if(depth>8)throw std::runtime_error("Ringbearer component depth");
        const bool wide=i16(head+50)!=0;auto location=[&](unsigned i){return wide?u32(loca+4*i):2*u16(loca+2*i);};
        auto off=location(id);if(off==location(id+1))return;size_t p=glyf+off;int n=i16(p);p+=10;
        if(n>=0){
            if(n>4096)throw std::runtime_error("Font contour limit");
            std::vector<unsigned> ends;for(int j=0;j<n;++j){ends.push_back(u16(p));p+=2;}if(ends.empty())return;
            unsigned count=ends.back()+1;if(count>32768)throw std::runtime_error("Ringbearer point count");p+=2+u16(p);
            work(count);std::vector<unsigned> flags;while(flags.size()<count){unsigned f=u8(p++);flags.push_back(f);if(f&8){unsigned repeat=u8(p++);if(flags.size()+repeat>count)throw std::runtime_error("Ringbearer flags");while(repeat--)flags.push_back(f);}}
            std::vector<Point> points(count);int x=0,y=0;
            for(unsigned j=0;j<count;++j){unsigned f=flags[j];int dx=0;if(f&2){dx=u8(p++);if(!(f&16))dx=-dx;}else if(!(f&16)){dx=i16(p);p+=2;}x+=dx;points[j]={double(x),0,bool(f&1)};}
            for(unsigned j=0;j<count;++j){unsigned f=flags[j];int dy=0;if(f&4){dy=u8(p++);if(!(f&32))dy=-dy;}else if(!(f&32)){dy=i16(p);p+=2;}y+=dy;points[j].y=y;}
            unsigned first=0;for(auto last:ends){contour(points,first,last,t);first=last+1;}
        }else{
            unsigned flags;do{work();flags=u16(p);unsigned child=u16(p+2);p+=4;int x,y;
                if(flags&1){x=i16(p);y=i16(p+2);p+=4;}else{x=int8_t(u8(p));y=int8_t(u8(p+1));p+=2;}
                if(!(flags&2))throw std::runtime_error("Ringbearer point attachment unsupported");
                Transform q;q.x=x;q.y=y;
                if(flags&8){q.a=q.d=i16(p)/16384.;p+=2;}
                else if(flags&64){q.a=i16(p)/16384.;q.d=i16(p+2)/16384.;p+=4;}
                else if(flags&128){q.a=i16(p)/16384.;q.b=i16(p+2)/16384.;q.c=i16(p+4)/16384.;q.d=i16(p+6)/16384.;p+=8;}
                Transform u{t.a*q.a+t.c*q.b,t.b*q.a+t.d*q.b,t.a*q.c+t.c*q.d,t.b*q.c+t.d*q.d,t.a*q.x+t.c*q.y+t.x,t.b*q.x+t.d*q.y+t.y};outline(child,u,depth+1);
            }while(flags&32);
        }
    }
public:
    std::vector<Edge> edges;
    std::array<Glyph,96> glyphs{};
    unsigned units_per_em=2048;
    Font(){
#ifndef THIRD_AGE_PORTABLE_RELEASE
        load(std::vector<uint8_t>(std::begin(ringbearer_data),std::end(ringbearer_data)));
#endif
    }
    void load(const std::vector<uint8_t>& bytes){
        if(bytes.size()<12||bytes.size()>8*1024*1024)throw std::runtime_error("Font size outside limits");
        data_=bytes;work_=0;edges.clear();glyphs={};pairs.clear();
        if(u32(0)!=0x00010000||u16(4)>256||12+size_t(u16(4))*16>data_.size())throw std::runtime_error("Expected bounded TrueType font");
        for(unsigned j=0;j<u16(4);++j){size_t p=12+16*j;uint64_t end=uint64_t(u32(p+8))+u32(p+12);if(end>data_.size())throw std::runtime_error("Font table outside file");}
        head=table(0x68656164);loca=table(0x6c6f6361);glyf=table(0x676c7966);cmap=table(0x636d6170);hmtx=table(0x686d7478);hhea=table(0x68686561);
        if(!head||!loca||!glyf||!cmap||!hmtx||!hhea)throw std::runtime_error("Ringbearer tables");
        units_per_em=u16(head+18);if(units_per_em<16||units_per_em>16384)throw std::runtime_error("Font units outside limits");
        uint32_t sub=0;for(unsigned j=0;j<u16(cmap+2);++j){auto p=cmap+4+8*j,off=cmap+u32(p+4);if(u16(off)==4&&(u16(p)==0||(u16(p)==3&&u16(p+2)==1)))sub=off;}
        if(!sub)throw std::runtime_error("Ringbearer cmap");cmap=sub;unsigned metrics=u16(hhea+34);if(!metrics)throw std::runtime_error("Font has no metrics");
        for(unsigned j=0;j<96;++j){unsigned id=ids[j]=character(j==95?251:j+32),first=edges.size();outline(id,Transform{});glyphs[j]={first,unsigned(edges.size()-first),int(u16(hmtx+4*std::min(id,metrics-1)))};}
        auto kern=table(0x6b65726e);if(kern){size_t p=kern+4;for(unsigned j=0;j<u16(kern+2);++j){unsigned len=u16(p+2),coverage=u16(p+4);if(len<6)break;
            if(coverage==1){unsigned count=u16(p+6);for(unsigned k=0;k<count;++k){size_t at=p+14+6*k;pairs[(uint32_t(u16(at))<<16)|u16(at+2)]=i16(at+4);}}p+=len;}}
    }
    int kern(unsigned a,unsigned b)const{int ia=a>=32&&a<=126?int(a)-32:a==251?95:-1,ib=b>=32&&b<=126?int(b)-32:b==251?95:-1;if(ia<0||ib<0)return 0;
        auto it=pairs.find((ids[ia]<<16)|ids[ib]);return it==pairs.end()?0:it->second;}
};
inline Font ringbearer;
inline unsigned units_per_em(){return ringbearer.units_per_em;}
inline const auto& edges=ringbearer.edges;
inline const auto& glyphs=ringbearer.glyphs;
inline int kerning(unsigned a,unsigned b){return ringbearer.kern(a,b);}
}

#ifdef THIRD_AGE_PORTABLE_RELEASE
namespace gbarecomp::hd_font {
inline std::string initialize_external_font(const std::filesystem::path& executable){
    namespace fs=std::filesystem;
    std::vector<fs::path> candidates;
    // Explicit font is required if requested, so an invalid selection is visible.
    const char* selected=std::getenv("THIRD_AGE_FONT");
    if(selected&&*selected)candidates.push_back(path_from_utf8(selected));
    else {
        auto local=executable.parent_path()/"assets"/"fonts"/"ringbearer.ttf";
        std::error_code ec;if(fs::is_regular_file(local,ec))candidates.push_back(local);
#ifdef _WIN32
        const char* windir=std::getenv("WINDIR");
        const fs::path fonts=fs::path(windir&&*windir?windir:"C:/Windows")/"Fonts";
        candidates.push_back(fonts/"arial.ttf");candidates.push_back(fonts/"segoeui.ttf");
#else
        candidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        candidates.emplace_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
        candidates.emplace_back("/usr/share/fonts/liberation/LiberationSans-Regular.ttf");
        candidates.emplace_back("/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf");
#endif
    }
    std::string errors;
    for(const auto& path:candidates){
        try{
            std::ifstream input(path,std::ios::binary|std::ios::ate);
            if(!input)throw std::runtime_error("not readable");
            auto length=input.tellg();if(length<12||length>8*1024*1024)throw std::runtime_error("invalid size");
            std::vector<uint8_t> bytes(static_cast<size_t>(length));input.seekg(0);
            if(!input.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))throw std::runtime_error("read failed");
            Font loaded;loaded.load(bytes);ringbearer=std::move(loaded);
            return path_to_utf8(path);
        }catch(const std::exception& e){
            errors+=path_to_utf8(path)+": "+e.what()+"; ";
            if(selected&&*selected)break;
            std::fprintf(stderr,"Font candidate rejected: %s (%s)\n",path_to_utf8(path).c_str(),e.what());
        }
    }
    throw std::runtime_error("No supported system TrueType font. Set THIRD_AGE_FONT to a local TTF. "+errors);
}
}
#endif
