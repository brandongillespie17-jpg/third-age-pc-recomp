#pragma once
// Approved 2026-10-12 roster. Host presentation only; never writes guest memory.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>
namespace gbarecomp {
struct ThirdAgePortraitIdentity { unsigned kind,type; uint32_t resource; const char* slug; };
inline constexpr std::array<ThirdAgePortraitIdentity,68> third_age_portrait_ids{{
    {6,0,0x08f86dbcu,"aragorn"},
    {6,1,0x08f87380u,"gimli"},
    {6,2,0x08f87944u,"legolas"},
    {6,3,0x08f87f08u,"rohan-warrior"},
    {6,4,0x08f884a0u,"wood-elf-archer"},
    {6,5,0x08f88a34u,"eowyn"},
    {6,6,0x08f88ff4u,"haldir"},
    {6,7,0x08f895acu,"gondor-soldier"},
    {6,8,0x08f89b40u,"standard-bearer"},
    {6,9,0x08f8a0c8u,"gandalf"},
    {6,10,0x08f8a684u,"boromir"},
    {6,11,0x08f8abfcu,"pippin"},
    {6,12,0x08f8b184u,"gloin"},
    {6,13,0x08f8b6e4u,"woodsman"},
    {6,14,0x08f8bc7cu,"merry"},
    {6,15,0x08f8c22cu,"elrond"},
    {6,16,0x08f8c7acu,"high-elf-swordman"},
    {6,17,0x08f8cd70u,"theoden"},
    {6,18,0x08f8d324u,"theodred"},
    {6,19,0x08f8d864u,"gondorian-ranger"},
    {6,20,0x08f8dd54u,"peasant"},
    {6,21,0x08f8e318u,"ent"},
    {6,22,0x08f8e8dcu,"isildur"},
    {6,23,0x08f8ee60u,"rohan-rider"},
    {6,24,0x08f8f3c8u,"faramir"},
    {6,25,0x08f8f968u,"army-of-the-dead"},
    {6,26,0x08f8ff08u,"knight-of-gondor"},
    {6,27,0x08f90454u,"eomer"},
    {6,28,0x08f909f8u,"treebeard"},
    {6,29,0x08f90fb4u,"ballista"},
    {6,30,0x08f91538u,"trebuchet"},
    {6,31,0x08f91ab8u,"captain"},
    {5,0,0x08f7a4ccu,"gorbag"},
    {5,1,0x08f7aa8cu,"mordor-troll"},
    {5,2,0x08f7b050u,"morannon-orc"},
    {5,3,0x08f7b5e8u,"uruk-hai-berserker"},
    {5,4,0x08f7bbacu,"uruk-hai"},
    {5,5,0x08f7c16cu,"lurtz"},
    {5,6,0x08f7c70cu,"uruk-hai-pikeman"},
    {5,7,0x08f7cca4u,"shagrat"},
    {5,8,0x08f7d228u,"uruk-hai-flagbearer"},
    {5,9,0x08f7d76cu,"goblin"},
    {5,10,0x08f7dd18u,"dunlending"},
    {5,11,0x08f7e2dcu,"warg-rider"},
    {5,12,0x08f7e864u,"wormtongue"},
    {5,13,0x08f7ee00u,"saruman"},
    {5,14,0x08f7f3a4u,"mouth-of-sauron"},
    {5,15,0x08f7f90cu,"witch-king"},
    {5,16,0x08f7fea0u,"cave-troll"},
    {5,17,0x08f80458u,"mordor-uruk-hai"},
    {5,18,0x08f80984u,"ringwraith"},
    {5,19,0x08f80e8cu,"dark-rider"},
    {5,20,0x08f81408u,"mumakil"},
    {5,21,0x08f81968u,"uruk-hai-lieutenant"},
    {5,22,0x08f81e90u,"orc-chief"},
    {5,23,0x08f82424u,"chieftain"},
    {5,24,0x08f829d4u,"gothmog"},
    {5,25,0x08f82f70u,"ugluk"},
    {5,26,0x08f83510u,"uruk-hai-captain"},
    {5,27,0x08f83accu,"haradrim"},
    {5,28,0x08f84068u,"sharku"},
    {5,29,0x08f8461cu,"grishnakh"},
    {5,30,0x08f84bc0u,"orc-captain"},
    {5,31,0x08f8516cu,"easterling"},
    {5,32,0x08f8572cu,"uruk-ballista"},
    {5,33,0x08f85cd8u,"catapult"},
    {5,34,0x08f86240u,"sauron"},
    {5,35,0x08f86804u,"orc-brute"},
}};
inline int third_age_portrait_id(unsigned kind,unsigned type){
    for(size_t i=0;i<third_age_portrait_ids.size();++i){const auto& p=third_age_portrait_ids[i];if(p.kind==kind&&p.type==type)return int(i);}return -1;
}
inline int third_age_portrait_resource(uint32_t resource){
    if(resource==0x08f79bdcu)return 68; // Good flank with no commander.
    if(resource==0x08f79f14u)return 69; // Evil flank with no commander.
    for(size_t i=0;i<third_age_portrait_ids.size();++i)if(third_age_portrait_ids[i].resource==resource)return int(i);return -1;
}
inline bool third_age_original_portraits(){
    static const bool original=[](){const char* v=std::getenv("THIRD_AGE_ORIGINAL_PORTRAITS");return v&&std::string(v)=="1";}();return original;
}
struct ThirdAgePortraitImage {
    int size=0;std::vector<uint32_t> pixels;
    uint32_t sample(int x,int y,int output)const{
        // Bilinear sampling uses the full approved image, without cropping/mirroring.
        const double fx=std::clamp((x+.5)*size/output-.5,0.,double(size-1));
        const double fy=std::clamp((y+.5)*size/output-.5,0.,double(size-1));
        const int x0=int(fx),y0=int(fy),x1=std::min(size-1,x0+1),y1=std::min(size-1,y0+1);
        const double dx=fx-x0,dy=fy-y0;uint32_t out=0;
        for(int shift:{0,8,16,24}){
            auto c=[&](int xx,int yy){return double((pixels[yy*size+xx]>>shift)&255);};
            const double v=(c(x0,y0)*(1-dx)+c(x1,y0)*dx)*(1-dy)+(c(x0,y1)*(1-dx)+c(x1,y1)*dx)*dy;
            out|=uint32_t(v+.5)<<shift;
        }return out;
    }
};
class ThirdAgePortraitAssets {
    struct Entry {ThirdAgePortraitImage image;uint64_t age=0;};
    std::map<int,Entry> loaded_;std::array<bool,70> failed_{};uint64_t clock_=0;
    std::vector<std::filesystem::path> roots_;
    static bool decode(const std::filesystem::path& path,ThirdAgePortraitImage& out){
        std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)return false;
        const auto length=f.tellg();if(length<54||length>16*1024*1024)return false;
        std::vector<uint8_t> bytes(static_cast<size_t>(length));f.seekg(0);if(!f.read(reinterpret_cast<char*>(bytes.data()),length))return false;
        auto u16=[&](size_t i){return unsigned(bytes[i])|(unsigned(bytes[i+1])<<8);};
        auto u32=[&](size_t i){return uint32_t(u16(i))|(uint32_t(u16(i+2))<<16);};
        const uint32_t offset=u32(10),dib=u32(14),bits=u16(28);
        const int32_t w=int32_t(u32(18)),raw_h=int32_t(u32(22));
        if(bytes[0]!='B'||bytes[1]!='M'||dib<40||dib>bytes.size()-14||offset<14+dib||offset>bytes.size()||u16(26)!=1||u32(30)!=0||!(bits==24||bits==32)||w<1||w>2048||!(raw_h==w||raw_h==-w))return false;
        const size_t stride=(size_t(w)*(bits/8)+3)&~size_t(3);
        if(size_t(w)*stride>bytes.size()-offset)return false;
        ThirdAgePortraitImage next;next.size=w;next.pixels.resize(size_t(w)*w);
        for(int y=0;y<w;++y)for(int x=0;x<w;++x){
            const size_t i=offset+size_t(raw_h<0?y:w-1-y)*stride+x*(bits/8);
            next.pixels[y*w+x]=(bits==32?uint32_t(bytes[i+3])<<24:0xff000000u)|(uint32_t(bytes[i+2])<<16)|(uint32_t(bytes[i+1])<<8)|bytes[i];
        }out=std::move(next);return true;
    }
public:
    explicit ThirdAgePortraitAssets(const std::filesystem::path& executable_dir={}){
        // Explicit override is exclusive: missing files there use original art.
        if(const char* env=std::getenv("THIRD_AGE_ASSET_DIR");env&&*env){roots_.emplace_back(env);return;}
        auto add=[&](std::filesystem::path p){for(int i=0;i<6&&!p.empty();++i){roots_.push_back(p/"assets");auto parent=p.parent_path();if(parent==p)break;p=parent;}};
        add(executable_dir);std::error_code ec;auto cwd=std::filesystem::current_path(ec);if(!ec)add(cwd);
    }
    const ThirdAgePortraitImage* get(int id){
        if(third_age_original_portraits()||id<0||id>=70||failed_[id])return nullptr;
        if(auto it=loaded_.find(id);it!=loaded_.end()){it->second.age=++clock_;return &it->second.image;}
        Entry next;
        for(const auto& root:roots_)if(decode(root/"portraits"/(std::string(id==68?"faction-good":id==69?"faction-evil":third_age_portrait_ids[id].slug)+".bmp"),next.image)){
            if(loaded_.size()>=8){auto victim=std::min_element(loaded_.begin(),loaded_.end(),[](const auto& a,const auto& b){return a.second.age<b.second.age;});loaded_.erase(victim);}
            next.age=++clock_;return &loaded_.emplace(id,std::move(next)).first->second.image;
        }failed_[id]=true;return nullptr;
    }
};
}
