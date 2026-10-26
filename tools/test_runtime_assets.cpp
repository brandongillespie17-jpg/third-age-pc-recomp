// Developer-only regression harness. Inputs/reference headers are private and external.
#define THIRD_AGE_PORTABLE_RELEASE 1
#include "third_age_runtime_portraits.h"
#include "third_age_native_decode.h"
#include "third_age_hd_font.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <chrono>
#ifdef THIRD_AGE_REFERENCE_TEST
#include "reference_decode.h"
#include "reference_font.h"
namespace reference {
using ThirdAgePortraitProbe=gbarecomp::ThirdAgePortraitProbe;
#include "reference_portraits.inc"
}
#endif
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static std::vector<uint8_t> file(const char* p){std::ifstream f(p,std::ios::binary);require(bool(f),"Input missing");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){try{
    require(argc==3,"Usage: test_runtime_assets OWNED_ROM ORIGINAL_FONT");
    auto rom=file(argv[1]),font=file(argv[2]);require(rom.size()==16777216,"Expected ROM size");
    gbarecomp::initialize_runtime_portraits(rom.data(),rom.size());
    bool rejected=false;try{gbarecomp::initialize_runtime_portraits(rom.data(),128);}catch(const std::exception&){rejected=true;}
    require(rejected,"Truncated portrait ROM accepted");
    auto broken=rom;std::fill(broken.begin()+0xa0c188,broken.begin()+0xa0c190,0xff);
    rejected=false;try{gbarecomp::initialize_runtime_portraits(broken.data(),broken.size());}catch(const std::exception&){rejected=true;}
    require(rejected,"Out-of-range portrait table accepted");
#ifdef THIRD_AGE_REFERENCE_TEST
    unsigned tables=0;
    for(unsigned size:{40u,32u})for(unsigned i=0;i<68;++i){
        const auto& a=size==40?gbarecomp::third_age_portrait_patterns()[i]:gbarecomp::third_age_small_portrait_patterns()[i];
        const auto& b=size==40?reference::third_age_portrait_patterns()[i]:reference::third_age_small_portrait_patterns()[i];
        require(a.size()==b.size(),"Portrait size mismatch");
        for(unsigned j=0;j<a.size();++j)require(a[j].offset==b[j].offset&&a[j].rgb==b[j].rgb,"Portrait pixel/order mismatch");++tables;
    }
    std::cout<<"portrait_tables_exact="<<tables<<"\n";
#endif
    gbarecomp::hd_font::Font loaded;loaded.load(font);
#ifdef THIRD_AGE_REFERENCE_TEST
    const auto& original=reference::hd_font::ringbearer;
    require(loaded.edges.size()==original.edges.size(),"Font edge count mismatch");
    for(size_t j=0;j<loaded.edges.size();++j){auto a=loaded.edges[j];auto b=original.edges[j];
        require(a.x0==b.x0&&a.x1==b.x1&&a.y0==b.y0&&a.y1==b.y1,"Font outline mismatch");}
    for(unsigned j=0;j<96;++j){auto a=loaded.glyphs[j];auto b=original.glyphs[j];require(a.first==b.first&&a.count==b.count&&a.advance==b.advance,"Font glyph mismatch");}
    for(unsigned a=32;a<127;++a)for(unsigned b=32;b<127;++b)require(loaded.kern(a,b)==original.kern(a,b),"Font kerning mismatch");
    std::cout<<"font_glyphs_exact=96\n";
#endif
    for(size_t length:{size_t(0),size_t(11),size_t(128)}){rejected=false;try{loaded.load(std::vector<uint8_t>(length));}catch(const std::exception&){rejected=true;}require(rejected,"Invalid font accepted");}
    std::array<uint8_t,262144> ew{};std::array<uint8_t,32768> iw{};std::array<uint8_t,98304> vr{};
    std::copy_n(rom.data()+0x44c,0x32c,iw.data()+0x3b00);
    auto decoder=std::make_unique<gbarecomp::ThirdAgeNativeDecode>();
#ifdef THIRD_AGE_REFERENCE_TEST
    auto reference=std::make_unique<reference::ThirdAgeNativeDecode>();
#endif
    std::vector<size_t> candidates;
    auto word=[&](size_t at){return uint32_t(rom[at])|uint32_t(rom[at+1])<<8|uint32_t(rom[at+2])<<16|uint32_t(rom[at+3])<<24;};
    for(size_t at=0;at+8<rom.size();at+=4){auto w=word(at);if((w&255)==0x60&&(w>>8)>0&&(w>>8)<=16384&&!(w>>8&31)&&rom[at+4]>=4&&rom[at+4]<=32&&!(rom[at+4]&3))candidates.push_back(at);}
    require(candidates.size()>=64,"Too few decoder resources");unsigned successes=0;
    auto start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<64;++i){uint32_t regs[16]{};regs[0]=0x08000004+uint32_t(candidates[i*candidates.size()/64]);regs[1]=0x06010000;
        regs[13]=0x03007e00;regs[14]=0x08000000;regs[15]=0x03003b00;uint32_t flags=31u|((i&15)<<28);
        bool ok=decoder->evaluate(regs,flags,ew.data(),iw.data(),vr.data(),rom.data(),rom.size());successes+=ok;
#ifdef THIRD_AGE_REFERENCE_TEST
        bool ref=reference->evaluate(regs,flags,ew.data(),iw.data(),vr.data(),rom.data(),rom.size());require(ok==ref,"Decoder acceptance mismatch");
        if(ok){require(decoder->regs==reference->regs&&decoder->cpsr==reference->cpsr,"Decoder CPU mismatch");
            require(decoder->ew==reference->ew&&decoder->iw==reference->iw&&decoder->vr==reference->vr,"Decoder scratch memory mismatch");
            require(decoder->written==reference->written&&decoder->steps==reference->steps&&decoder->writes.size()==reference->writes.size(),"Decoder accounting mismatch");
            for(size_t j=0;j<decoder->writes.size();++j){auto a=decoder->writes[j];auto b=reference->writes[j];require(a.address==b.address&&a.value==b.value&&a.width==b.width,"Decoder write mismatch");}}
#endif
    }
    require(successes==64,"Expected all sampled resources to decode");
    uint32_t regs[16]{};regs[13]=0x03007e00;regs[14]=0x08000000;regs[15]=0x03003b00;
    require(!decoder->evaluate(regs,31,ew.data(),iw.data(),vr.data(),rom.data(),rom.size()),"Invalid decoder source accepted");
    iw[0x3b00]^=1;require(!decoder->evaluate(regs,31,ew.data(),iw.data(),vr.data(),rom.data(),rom.size()),"Changed decoder code accepted");
    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"decoder_resources_exact="<<successes<<" elapsed_ms="<<ms<<"\nmalformed_inputs=rejected\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
