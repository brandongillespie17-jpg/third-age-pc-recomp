#pragma once
#include "third_age_unit_panel.h"
#include "third_age_command_panel.h"
#include "third_age_battle_menu.h"
#include "third_age_panel_theme.h"
#include "third_age_hd_font.h"
#include "third_age_portrait_assets.h"
#include <cmath>
#include <vector>
namespace gbarecomp {
// SDL-only output-resolution panel. Outline rasterization is cached until its
// contents or output size change. Drawing never alters renderer layout/input.
class ThirdAgeHdPanel {
    SDL_Texture *card_=nullptr,*underlay_=nullptr,*highlight_=nullptr;
    std::string highlight_key_;
    int portrait_id_=-1;
    bool hd_portraits_=true;
    static ThirdAgePortraitAssets& approved_assets(){
        static ThirdAgePortraitAssets assets([](){char* path=SDL_GetBasePath();std::filesystem::path p=path?path:"";SDL_free(path);return p;}());return assets;
    }
    bool approved_portrait(int id,int px,int py,int picture){
        if(!hd_portraits_)return false;
        const auto* art=approved_assets().get(id);if(!art)return false;
        for(int y=0;y<picture;++y)for(int x=0;x<picture;++x){auto c=art->sample(x,y,picture);if(c>>24)pixel(px+x,py+y,c,(c>>24)/255.);}
        return true;
    }
    int width_=0,height_=0;
    std::string menu_key_;
    std::string name_;int hp_=-1,max_hp_=-1,side_=-1,attack_=-1,range_=-1,movement_=-1;bool portrait_visible_=false,left_=false;
    std::array<uint32_t,1024> portrait_{};
    std::vector<uint32_t> pixels_;
    ThirdAgePanelTheme theme_;
    bool command_mode_=false,terrain_mode_=false;
    int defense_=0;
    ThirdAgeTurnStyle turn_style_=ThirdAgeTurnStyle::Normal;
    std::array<uint32_t,1600> command_portrait_{};
    static const hd_font::Glyph* glyph(unsigned char c){
        if(c>=32&&c<=126)return &hd_font::glyphs[c-32];
        if(c==251)return &hd_font::glyphs[95];
        return nullptr;
    }
    static double text_width(const std::string& s,double size){
        double w=0;unsigned prev=0;for(unsigned char c:s){w+=hd_font::kerning(prev,c)*size/double(hd_font::units_per_em());if(c==1)w+=size;else if(auto g=glyph(c))w+=g->advance*size/double(hd_font::units_per_em());prev=c;}return w;
    }
    void pixel(int x,int y,uint32_t color,double alpha=1){
        if(x<0||y<0||x>=width_||y>=height_)return;
        auto& dst=pixels_[y*width_+x];
        const double da=(dst>>24)/255.,oa=alpha+da*(1-alpha);if(oa<=0)return;
        uint32_t out=uint32_t(std::lround(255*oa))<<24;
        for(int shift:{0,8,16})out|=uint32_t(std::clamp(int((((color>>shift)&255)*alpha+((dst>>shift)&255)*da*(1-alpha))/oa+.5),0,255))<<shift;
        dst=out;
    }
    void rect(int x,int y,int w,int h,uint32_t color){
        if(w<=0||h<=0)return;
        for(int yy=std::max(0,y);yy<std::min(height_,y+h);++yy)
            std::fill(pixels_.begin()+yy*width_+std::clamp(x,0,width_),pixels_.begin()+yy*width_+std::clamp(x+w,0,width_),color);
    }
    void text(const std::string& value,double x,double baseline,double size,uint32_t color,uint32_t outline=0,int radius=0){
        const double scale=size/double(hd_font::units_per_em());unsigned prev=0;
        for(unsigned char c:value){x+=hd_font::kerning(prev,c)*scale;prev=c;
            if(c==1){icon(3,int(std::lround(x)),int(std::lround(baseline-size*.82)),std::max(1,int(std::lround(size))),color);x+=size;continue;}
            auto g=glyph(c);if(!g)continue;
            // Rasterize only this glyph's bounds. The old full-canvas scratch
            // buffer made a pause highlight hundreds of megapixels of work.
            double minx=1e9,maxx=-1e9,miny=1e9,maxy=-1e9;
            for(unsigned j=0;j<g->count;++j){const auto& e=hd_font::edges[g->first+j];
                minx=std::min(minx,double(std::min(e.x0,e.x1)));maxx=std::max(maxx,double(std::max(e.x0,e.x1)));
                miny=std::min(miny,double(std::min(e.y0,e.y1)));maxy=std::max(maxy,double(std::max(e.y0,e.y1)));}
            if(!g->count){x+=g->advance*scale;continue;}
            const int x0=std::clamp(int(std::floor(x+minx*scale))-radius,0,width_),x1=std::clamp(int(std::ceil(x+maxx*scale))+radius,0,width_);
            const int y0=std::clamp(int(std::floor(baseline-maxy*scale))-radius,0,height_),y1=std::clamp(int(std::ceil(baseline-miny*scale))+radius,0,height_);
            const int w=x1-x0,h=y1-y0;if(w<=0||h<=0){x+=g->advance*scale;continue;}
            std::vector<double> coverage(size_t(w)*h,0),cross;
            for(int yy=y0;yy<y1;++yy)for(int sub=0;sub<4;++sub){
                const double fy=(baseline-(yy+(sub+.5)/4))/scale;cross.clear();
                for(unsigned j=0;j<g->count;++j){const auto& e=hd_font::edges[g->first+j];
                    if((e.y0<=fy&&fy<e.y1)||(e.y1<=fy&&fy<e.y0))
                        cross.push_back(x+(e.x0+(fy-e.y0)*(e.x1-e.x0)/(e.y1-e.y0))*scale);}
                std::sort(cross.begin(),cross.end());
                for(size_t j=0;j+1<cross.size();j+=2){const double a=cross[j],b=cross[j+1];
                    for(int xx=std::max(x0,int(std::floor(a)));xx<std::min(x1,int(std::ceil(b)));++xx)
                        coverage[(yy-y0)*w+xx-x0]+=std::max(0.,std::min(b,xx+1.)-std::max(a,double(xx)))/4;}
            }
            if(radius>0){std::vector<double> halo(coverage.size(),0);
                for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx){const double a=coverage[yy*w+xx];if(a<=0)continue;
                    for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx){
                        if(dx*dx+dy*dy>radius*radius||xx+dx<0||xx+dx>=w||yy+dy<0||yy+dy>=h)continue;
                        auto& v=halo[(yy+dy)*w+xx+dx];v=std::max(v,a);}}
                for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx)if(halo[yy*w+xx]>0)pixel(x0+xx,y0+yy,outline,std::min(1.,halo[yy*w+xx]));
            }
            for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx)if(coverage[yy*w+xx]>0)pixel(x0+xx,y0+yy,color,std::min(1.,coverage[yy*w+xx]));
            x+=g->advance*scale;
        }
    }
    static bool polygon(double x,double y,std::initializer_list<std::pair<double,double>> points){
        bool in=false;auto previous=*(points.end()-1);
        for(auto next:points){if((previous.second>y)!=(next.second>y)&&x<(next.first-previous.first)*(y-previous.second)/(next.second-previous.second)+previous.first)in=!in;previous=next;}return in;
    }
    static bool stroke(double x,double y,double ax,double ay,double bx,double by,double width){
        const double dx=bx-ax,dy=by-ay,t=std::clamp(((x-ax)*dx+(y-ay)*dy)/(dx*dx+dy*dy),0.,1.);
        const double ex=x-ax-t*dx,ey=y-ay-t*dy;return ex*ex+ey*ey<=width*width/4;
    }
    static bool icon_shape(int kind,double x,double y){
        if(kind==3)return polygon(x,y,{{12,1},{15,8},{22,12},{15,15},{12,23},{9,15},{2,12},{9,8}});
        if(kind==4)return stroke(x,y,5,2,19,2,2)||stroke(x,y,5,22,19,22,2)||
            polygon(x,y,{{6,4},{18,4},{17,8},{13,12},{17,16},{18,20},{6,20},{7,16},{11,12},{7,8}});
        if(kind==0)return polygon(x,y,{{10,16},{10,5},{12,1},{14,5},{14,16}})||
            stroke(x,y,5,17,19,17,2)||stroke(x,y,12,17,12,22,2.5)||stroke(x,y,10,22,14,22,2);
        if(kind==1){
            if(stroke(x,y,4,2,4,22,1)||stroke(x,y,1,12,22,12,1.5)||stroke(x,y,18,8,22,12,1.5)||stroke(x,y,18,16,22,12,1.5))return true;
            double ax=4,ay=2;
            for(int j=1;j<=12;++j){double t=j/12.,bx=4+40*t*(1-t),by=2+20*t;if(stroke(x,y,ax,ay,bx,by,2))return true;ax=bx;ay=by;}return false;
        }
        // A footprint: four toes and a curved sole, original vector geometry.
        if(polygon(x,y,{{6,10},{10,8},{15,10},{16,14},{13,18},{12,22},{8,23},{5,21},{5,16}}))return true;
        for(auto toe:std::initializer_list<std::array<double,4>>{{15,4,2.5,3},{10,3,2,2.4},{6,5,1.7,2},{3.5,8,1.4,1.7}}){double dx=(x-toe[0])/toe[2],dy=(y-toe[1])/toe[3];if(dx*dx+dy*dy<=1)return true;}return false;
    }
    void icon(int kind,int x,int y,int size,uint32_t color){
        // Supersampled vector silhouettes scale with output resolution.
        for(int yy=0;yy<size;++yy)for(int xx=0;xx<size;++xx){unsigned covered=0;
            for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx)covered+=icon_shape(kind,(xx+(sx+.5)/4)*24/size,(yy+(sy+.5)/4)*24/size);
            if(covered)pixel(x+xx,y+yy,color,covered/16.);
        }
    }
    bool prepare(SDL_Renderer* renderer,const ThirdAgeUnitPanel& p,int width,int height,bool left,const ThirdAgePanelTheme& theme){
        const int portrait_id=third_age_portrait_id(p.kind,p.type);
        if(card_&&terrain_mode_==p.terrain&&defense_==p.defense&&portrait_id_==portrait_id&&!command_mode_&&theme_==theme&&left_==left&&width==width_&&height==height_&&name_==p.name&&hp_==p.hp&&max_hp_==p.max_hp&&side_==p.side&&attack_==p.attack&&range_==p.range&&movement_==p.movement&&portrait_visible_==p.has_portrait&&portrait_==p.portrait)return true;
        if(card_)SDL_DestroyTexture(card_);
        card_=nullptr;
        width_=width;height_=height;pixels_.assign(size_t(width)*height,0xff15201b);
        const double s=height/112.;auto n=[&](double v){return std::max(1,int(std::lround(v*s)));};
        using T=ThirdAgePanelTheme;
        for(int y=0;y<height;++y){uint32_t color=0xff000000;for(unsigned shift:{0u,8u,16u}){
            const int top=(theme[T::BackgroundTop]>>shift)&255,bottom=(theme[T::BackgroundBottom]>>shift)&255;
            color|=uint32_t(top+(bottom-top)*y/std::max(1,height-1))<<shift;
        }rect(0,y,width,1,color);}
        const int border=n(1),pad=n(14),picture=n(80),px=left?pad:width-pad-picture;
        const int statx=left?px+picture+n(14):px-n(78);
        const int tx=left?statx+n(78):pad;
        const int available=left?width-pad-tx:statx-n(12)-tx;
        rect(0,0,width,border,theme[T::BorderLight]);rect(0,height-border,width,border,theme[T::BorderDark]);
        rect(0,0,border,height,theme[T::BorderDark]);rect(width-border,0,border,height,theme[T::BorderDark]);
        if(p.terrain){
            text(p.name,pad,36*s,23*s,theme[T::NameText]);
            const std::string move=p.movement>=200?"Blocked":p.movement>2?"Slowed":"Clear";
            text("Movement",pad,65*s,16*s,theme[T::HealthText]);
            text(move,180*s,65*s,18*s,theme[T::StatText]);
            const std::string defense=p.defense>0?"+"+std::to_string(p.defense):p.defense<0?std::to_string(p.defense):"None";
            text("Ranged defense",pad,92*s,16*s,theme[T::HealthText]);
            text(defense,180*s,92*s,18*s,theme[T::StatText]);
        }else{
        rect(px-border,n(16)-border,picture+2*border,picture+2*border,theme[T::PortraitBorder]);
        rect(px,n(16),picture,picture,theme[T::BackgroundBottom]);
        if(!approved_portrait(portrait_id,px,n(16),picture)&&p.has_portrait)for(int y=0;y<picture;++y)for(int x=0;x<picture;++x)pixel(px+x,n(16)+y,p.portrait[(y*32/picture)*32+x*32/picture]);
        double name_size=23*s;
        if(text_width(p.name,name_size)>available)name_size*=available/text_width(p.name,name_size);
        text(p.name,tx,36*s,name_size,theme[T::NameText]);
        const std::string health="Health  "+std::to_string(p.hp)+" / "+std::to_string(p.max_hp);
        text(health,tx,64*s,16*s,theme[T::HealthText]);
        const int by=n(78),bh=n(9);
        if(p.max_hp>0){
            rect(tx,by,available,bh,theme[T::HealthTrack]);
            // Unit side is kind - 5: 1 = Good, 0 = Evil. Keep faction colors at low HP.
            const uint32_t fill=p.side==1?0xff89cff0u:0xffb01020u;
            rect(tx+border,by+border,std::max(0,int((available-2*border)*double(p.hp)/p.max_hp)),std::max(1,bh-2*border),fill);
        }
        const int stats[]={p.attack,p.range,p.movement};
        for(int i=0;i<3;++i){
            icon(i,statx,n(16+28*i),n(24),theme[T::Part(T::SwordIcon+i)]);
            const auto value=std::to_string(stats[i]);double size=20*s;
            const double room=n(64)-n(34);if(text_width(value,size)>room)size*=room/text_width(value,size);
            text(value,statx+n(34),(36+28*i)*s,size,theme[T::StatText]);
        }
        }
        SDL_Texture* next=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,width,height);
        if(!next)return false;
        if(SDL_UpdateTexture(next,nullptr,pixels_.data(),width*4)!=0){SDL_DestroyTexture(next);return false;}
        SDL_SetTextureBlendMode(next,SDL_BLENDMODE_NONE);
        card_=next;
        terrain_mode_=p.terrain;defense_=p.defense;portrait_id_=portrait_id;command_mode_=false;theme_=theme;left_=left;name_=p.name;hp_=p.hp;max_hp_=p.max_hp;side_=p.side;attack_=p.attack;range_=p.range;movement_=p.movement;portrait_=p.portrait;portrait_visible_=p.has_portrait;return true;
    }
    bool prepare_command(SDL_Renderer* renderer,const ThirdAgeCommandPanel& p,int width,int height,const ThirdAgePanelTheme& theme){
        const int portrait_id=third_age_portrait_resource(p.portrait_source);
        if(card_&&portrait_id_==portrait_id&&command_mode_&&turn_style_==p.turn_style&&theme_==theme&&width_==width&&height_==height&&name_==p.name&&hp_==p.spirit&&max_hp_==p.max_spirit&&attack_==p.turns&&range_==int(p.flank)&&portrait_visible_==p.has_portrait&&command_portrait_==p.portrait)return true;
        if(card_)SDL_DestroyTexture(card_);card_=nullptr;
        width_=width;height_=height;pixels_.assign(size_t(width)*height,0xff15201b);
        const double s=height/156.;auto n=[&](double v){return std::max(1,int(std::lround(v*s)));};
        using T=ThirdAgePanelTheme;
        for(int y=0;y<height;++y){uint32_t color=0xff000000;for(unsigned shift:{0u,8u,16u}){
            const int top=(theme[T::BackgroundTop]>>shift)&255,bottom=(theme[T::BackgroundBottom]>>shift)&255;
            color|=uint32_t(top+(bottom-top)*y/std::max(1,height-1))<<shift;
        }rect(0,y,width,1,color);}
        const int border=n(1),pad=n(12),picture=n(128),px=pad,py=n(14),tx=n(154);
        rect(0,0,width,border,theme[T::BorderLight]);rect(0,height-border,width,border,theme[T::BorderDark]);
        rect(0,0,border,height,theme[T::BorderDark]);rect(width-border,0,border,height,theme[T::BorderDark]);
        rect(px-border,py-border,picture+2*border,picture+2*border,theme[T::PortraitBorder]);
        rect(px,py,picture,picture,theme[T::BackgroundBottom]);
        if(!approved_portrait(portrait_id,px,py,picture)&&p.has_portrait)for(int y=0;y<picture;++y)for(int x=0;x<picture;++x){auto c=p.portrait[(y*40/picture)*40+x*40/picture];if(c>>24)pixel(px+x,py+y,c);}
        for(int row=0;row<2;++row){
            const int y=29+row*64;
            icon(3+row,tx,n(y-3),n(42),theme[row?T::TurnIcon:T::SpiritIcon]);
            const auto value=std::to_string(row?p.turns:p.spirit);
            double size=48*s;const double available=width-pad-(tx+n(48))-n(3);
            if(text_width(value,size)>available)size*=available/text_width(value,size);
            const bool marked=row&&p.turn_style!=ThirdAgeTurnStyle::Normal;
            const auto part=T::Part(T::FreeMoveOutline+std::max(0,int(p.turn_style)-1));
            text(value,tx+n(48),(y+35)*s,size,theme[row?T::TurnText:T::SpiritText],
                 marked?theme[part]:0,marked?n(2):0);
        }
        auto* next=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,width,height);
        if(!next)return false;
        if(SDL_UpdateTexture(next,nullptr,pixels_.data(),width*4)!=0){SDL_DestroyTexture(next);return false;}
        SDL_SetTextureBlendMode(next,SDL_BLENDMODE_NONE);card_=next;portrait_id_=portrait_id;command_mode_=true;turn_style_=p.turn_style;theme_=theme;
        name_=p.name;hp_=p.spirit;max_hp_=p.max_spirit;attack_=p.turns;range_=p.flank;portrait_visible_=p.has_portrait;command_portrait_=p.portrait;return true;
    }
    std::vector<std::string> wrapped(const std::string& value,double size,double room)const{
        std::vector<std::string> lines;std::string line,word;
        auto flush=[&](){if(word.empty())return;if(!line.empty()&&text_width(line+" "+word,size)>room){lines.push_back(line);line.clear();}if(!line.empty())line+=' ';line+=word;word.clear();};
        for(char c:value){if(c==' '||c=='\n'){flush();if(c=='\n'){lines.push_back(line);line.clear();}}else word+=c;}
        flush();if(!line.empty())lines.push_back(line);return lines;
    }
    bool menu_canvas(SDL_Renderer* r,const std::string& key,int width,int height,const ThirdAgePanelTheme& theme){
        if(card_&&menu_key_==key&&width_==width&&height_==height&&theme_==theme)return false;
        if(card_)SDL_DestroyTexture(card_);card_=nullptr;menu_key_=key;theme_=theme;width_=width;height_=height;
        pixels_.assign(size_t(width)*height,theme[ThirdAgePanelTheme::BackgroundBottom]);return true;
    }
    void menu_frame(double s,const ThirdAgePanelTheme& theme){
        using T=ThirdAgePanelTheme;const int border=std::max(1,int(std::lround(s/3)));
        for(int y=0;y<height_;++y){uint32_t color=0xff000000;for(unsigned shift:{0u,8u,16u}){
            const int top=(theme[T::BackgroundTop]>>shift)&255,bottom=(theme[T::BackgroundBottom]>>shift)&255;
            color|=uint32_t(top+(bottom-top)*y/std::max(1,height_-1))<<shift;
        }rect(0,y,width_,1,color);}
        rect(0,0,width_,border,theme[T::BorderLight]);rect(0,height_-border,width_,border,theme[T::BorderDark]);
        rect(0,0,border,height_,theme[T::BorderDark]);rect(width_-border,0,border,height_,theme[T::BorderDark]);
    }
    bool menu_upload(SDL_Renderer* r){
        card_=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,width_,height_);
        if(!card_)return false;
        if(SDL_UpdateTexture(card_,nullptr,pixels_.data(),width_*4)!=0){SDL_DestroyTexture(card_);card_=nullptr;return false;}
        SDL_SetTextureBlendMode(card_,SDL_BLENDMODE_NONE);return true;
    }
public:
    void draw_menu(SDL_Renderer* r,const ThirdAgeBattleMenu& p,const SDL_Rect& game,const ThirdAgePanelTheme& theme){
        if(!p.valid||!p.opacity||game.w<320||game.h<180)return;
        int lw=0,lh=0;SDL_RenderGetLogicalSize(r,&lw,&lh);if(lw||lh)return;
        const auto q=third_age_menu_layout(p);
        const double s=game.h/180.;auto n=[&](double v){return int(std::lround(v*s));};
        const int w=n(q.w),h=n(q.h);
        // Highlight is an inexpensive separate draw, never a text-raster cache key.
        std::string key="menu:"+std::to_string(p.paragraph)+":"+std::to_string(p.question)+p.pause_title+p.pause_objectives+p.pause_score;
        for(const auto& row:q.rows)key+='|'+row.text+":"+std::to_string(row.x)+":"+std::to_string(row.y)+":"+std::to_string(row.disabled);
        if(menu_canvas(r,key,w,h,theme)){
            using T=ThirdAgePanelTheme;
            if(p.pause){
                std::fill(pixels_.begin(),pixels_.end(),0); // Original art remains below.
                const auto ink=theme[T::NameText];
                // A clear gold rule and dark shadow separate title and scenery.
                rect(0,n(23),w,std::max(1,n(.8)),theme[T::BorderLight]);
                rect(0,n(23.8),w,std::max(1,n(1.4)),0xff12180e);
                rect(0,n(25.2),w,std::max(1,n(.35)),theme[T::BorderDark]);
                auto centered=[&](const std::string& v,double x,double y,double size){text(v,x*s-text_width(v,size*s)/2,y*s,size*s,ink,0xff10130c,std::max(1,n(.3)));};
                centered(p.pause_title,120,17,9.5);
                double size=7*s;auto lines=wrapped(p.pause_objectives,size,224*s);
                while(lines.size()*size*1.35>71*s&&size>3*s){size*=.94;lines=wrapped(p.pause_objectives,size,224*s);}
                double baseline=35*s;for(const auto& line:lines){text(line,8*s,baseline,size,theme[T::HealthText],0xff0b1008,std::max(1,n(.4)));baseline+=size*1.35;}
                centered(std::to_string(p.scores[0]),34,117,7.2);
                centered(std::to_string(p.scores[1]),206,117,7.2);
                centered(std::to_string(p.scores[2]),120,114,7.2);
                centered("Turn "+std::to_string(p.scores[3]),120,128,7);
            }else menu_frame(s,theme);
            if(p.paragraph){
                const bool footer=false;const auto& content=q.rows[0].text;
                double size=q.font*s;std::vector<std::string> lines;
                for(int attempt=0;attempt<25;++attempt){lines=wrapped(content,size,w-8*s);
                    double longest=0;for(const auto& line:lines)longest=std::max(longest,text_width(line,size));
                    if(lines.size()*size*1.3<=(q.h-(footer?17:6))*s&&longest<=w-8*s)break;size*=.94;
                }
                double baseline=3*s+size;
                for(const auto& line:lines){text(line,4*s,baseline,size,theme[T::HealthText]);baseline+=size*1.3;}
                if(footer)text("Click / A to continue",6*s,(q.h-4)*s,4*s,theme[T::BorderLight]);
            }else for(size_t i=0;i<q.rows.size();++i){const auto& row=q.rows[i];
                double room=q.w-row.x-5;
                if(p.question&&(i>0||p.grid)){for(size_t j=p.grid?0:1;j<q.rows.size();++j)if(q.rows[j].y==row.y&&q.rows[j].x>row.x)room=std::min(room,double(q.rows[j].x-row.x-4));}
                double size=(p.pause?7.4:q.font)*s;if(text_width(row.text,size)>room*s)size*=room*s/text_width(row.text,size);
                uint32_t color=theme[T::HealthText];
                if(row.disabled){uint32_t muted=0xff000000;for(int shift:{0,8,16})muted|=uint32_t((((color>>shift)&255)+((theme[T::BackgroundBottom]>>shift)&255))/2)<<shift;color=muted;}
                text(row.text,row.x*s,(row.y+(p.pause?7:6))*s,size,color,p.pause?0xff10130c:0,p.pause?std::max(1,n(.25)):0);
            }
            if(!menu_upload(r))return;
            SDL_SetTextureBlendMode(card_,SDL_BLENDMODE_BLEND);
        }
        SDL_SetTextureAlphaMod(card_,p.opacity);
        const SDL_Rect dst{game.x+n(q.x+p.offset_x),game.y+n(q.y+p.offset_y),w,h};SDL_RenderCopy(r,card_,nullptr,&dst);
        if(p.selected>=0&&p.selected<int(q.rows.size())){
            const auto& row=q.rows[p.selected];
            if(!p.paragraph){
                // Only rasterize the selected line when it changes. Keep the
                // menu backdrop/text cache independent of pointer navigation.
                double size=(p.pause?7.4:q.font)*s,room=(q.w-row.x-5)*s;
                if(p.question){for(size_t j=p.grid?0:1;j<q.rows.size();++j)
                    if(q.rows[j].y==row.y&&q.rows[j].x>row.x)room=std::min(room,(q.rows[j].x-row.x-4)*s);}
                const double baseline=p.pause?8:7;
                if(text_width(row.text,size)>room)size*=room/text_width(row.text,size);
                const auto key=row.text+":"+std::to_string(size)+":"+std::to_string(w)+":"+std::to_string(row.x)+":"+std::to_string(baseline);
                if(!highlight_||key!=highlight_key_){
                    if(highlight_)SDL_DestroyTexture(highlight_);
                    highlight_=nullptr;
                    ThirdAgeHdPanel glyphs;glyphs.width_=w;glyphs.height_=n(10);glyphs.pixels_.assign(size_t(w)*glyphs.height_,0);
                    glyphs.text(row.text,row.x*s,baseline*s,size,0xffffffff);
                    highlight_=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,w,glyphs.height_);
                    if(highlight_){SDL_UpdateTexture(highlight_,nullptr,glyphs.pixels_.data(),w*4);SDL_SetTextureBlendMode(highlight_,SDL_BLENDMODE_BLEND);highlight_key_=key;}
                }
                if(highlight_){
                    // Selection needs its own gold, independent of the normal
                    // title/text color. Unavailable choices retain a muted gold.
                    const uint32_t gold=row.disabled?0xb88f42u:0xffd652u;
                    SDL_SetTextureColorMod(highlight_,(gold>>16)&255,(gold>>8)&255,gold&255);
                    SDL_SetTextureAlphaMod(highlight_,p.opacity);
                    SDL_Rect line{dst.x,dst.y+n(row.y-1),w,n(10)};SDL_RenderCopy(r,highlight_,nullptr,&line);
                }
            }
        }
    }
    void draw_help(SDL_Renderer* r,const ThirdAgeBattleMenu& p,const SDL_Rect& game,const ThirdAgePanelTheme& theme,bool compact=true){
        if(!p.valid||!p.opacity||p.help.empty()||game.w<320||game.h<180)return;
        int lw=0,lh=0;SDL_RenderGetLogicalSize(r,&lw,&lh);if(lw||lh)return;
        const double s=game.h/180.;auto n=[&](double v){return int(std::lround(v*s));};
        const auto q=compact?third_age_menu_layout(p):ThirdAgeMenuLayout{p.x,p.y,p.w,p.h,6.5,p.rows};
        const int mx=q.x+p.offset_x,my=q.y+p.offset_y;
        int x=4,y=std::max(26,my),w=std::min(112,mx-10);
        // Prefer the free space next to the menu; fall back above it when
        // a wide centered dialog leaves insufficient room on either side.
        if(w<85){x=mx+q.w+6;w=std::min(112,316-x);}
        bool above=w<85;if(above){x=std::clamp(mx,4,204);w=112;}
        auto lines=wrapped(p.help,5.2*s,(w-12)*s);int h=20+int(std::ceil(lines.size()*6.8));
        if(above)y=std::max(24,my-h-6);else y=std::min(y,std::max(24,150-h));
        const auto key=p.help_title+'\n'+p.help+":cost:"+std::to_string(p.help_cost);
        if(menu_canvas(r,key,n(w),n(h),theme)){
            using T=ThirdAgePanelTheme;menu_frame(s,theme);double title_size=6*s;
            const std::string cost=p.help_cost>=0?std::string("\x01 ")+std::to_string(p.help_cost):std::string{};
            const double cost_width=text_width(cost,6*s),title_room=(w-12)*s-(cost.empty()?0:cost_width+5*s);
            if(text_width(p.help_title,title_size)>title_room)title_size*=title_room/text_width(p.help_title,title_size);
            text(p.help_title,6*s,10*s,title_size,theme[T::NameText]);
            if(!cost.empty())text(cost,(w-6)*s-cost_width,10*s,6*s,theme[T::SpiritText]);
            rect(n(6),n(14),n(w-12),std::max(1,n(.25)),theme[T::BorderDark]);
            double baseline=22*s;for(const auto& line:lines){text(line,6*s,baseline,5.2*s,theme[T::HealthText]);baseline+=6.8*s;}
            if(!menu_upload(r))return;
        }
        SDL_SetTextureBlendMode(card_,SDL_BLENDMODE_BLEND);SDL_SetTextureAlphaMod(card_,p.opacity);
        SDL_Rect dst{game.x+n(x),game.y+n(y),n(w),n(h)};SDL_RenderCopy(r,card_,nullptr,&dst);
    }
    static void host_text(SDL_Renderer* r,float x,float y,const std::string& value,double size,SDL_Color color,bool pixel_aligned=false){
        struct Cached {int w,h;std::vector<uint32_t> pixels;};
        static std::unordered_map<std::string,Cached> cache;
        float sx=1,sy=1;SDL_RenderGetScale(r,&sx,&sy);
        const double density=std::max(1.,std::max(double(sx),double(sy)));
        // Rasterize the unchanged outline font at drawable-pixel density.
        // The destination stays in the caller's coordinates, including HiDPI.
        const double px=size*density;
        std::string v=value;for(char& c:v)if(c>='A'&&c<='Z')c+=32;
        const auto key=std::to_string(px)+":"+v;auto it=cache.find(key);
        if(it==cache.end()){
            if(cache.size()>=256)cache.clear();
            double font=10.8*px;const int room=std::max(1,int(std::ceil(value.size()*6*px)));
            if(text_width(v,font)>room-2)font*=std::max(1,room-2)/text_width(v,font);
            ThirdAgeHdPanel p;p.width_=std::max(1,std::min(room,int(std::ceil(text_width(v,font)))+2));p.height_=int(std::ceil(8*px))+2;
            p.pixels_.assign(size_t(p.width_)*p.height_,0);p.text(v,1,7*px,font,0xffffffff);
            it=cache.emplace(key,Cached{p.width_,p.height_,std::move(p.pixels_)}).first;
        }
        const auto& c=it->second;auto* t=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,c.w,c.h);if(!t)return;
        if(SDL_UpdateTexture(t,nullptr,c.pixels.data(),c.w*4)==0){SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);SDL_SetTextureColorMod(t,color.r,color.g,color.b);SDL_SetTextureAlphaMod(t,color.a);
#if SDL_VERSION_ATLEAST(2,0,12)
            SDL_SetTextureScaleMode(t,pixel_aligned?SDL_ScaleModeNearest:SDL_ScaleModeLinear);
#endif
            // The font is already antialiased at output density. Moving its
            // bitmap through a second linear filter softens/churns the edges.
            // Snap only floating combat labels; other host text is unchanged.
            if(pixel_aligned){x=std::round(x*sx)/sx;y=std::round(y*sy)/sy;}
            SDL_FRect dst{x,y,float(c.w/density),float(c.h/density)};SDL_RenderCopyF(r,t,nullptr,&dst);}
        SDL_DestroyTexture(t);
    }
    // Fixed-size proportional labels for host UI fields. Alignment uses font
    // advances, never character count or a per-string font-size adjustment.
    static double label_width(std::string v,double font){
        for(char& c:v)if(c>='A'&&c<='Z')c+=32;
        return text_width(v,font);
    }
    static void host_label(SDL_Renderer* r,const SDL_Rect& box,const std::string& value,double font,int align,SDL_Color color){
        if(value.empty()||box.w<=0||box.h<=0)return;
        struct Cached {int w,h;double advance,baseline;std::vector<uint32_t> pixels;};
        static std::unordered_map<std::string,Cached> cache;
        float sx=1,sy=1;SDL_RenderGetScale(r,&sx,&sy);
        const double density=std::max(1.,std::max(double(sx),double(sy))),px=font*density;
        std::string v=value;for(char& c:v)if(c>='A'&&c<='Z')c+=32;
        const auto key=std::to_string(px)+":"+v;auto it=cache.find(key);
        const double pad=std::ceil(px*.15)+2,advance=text_width(v,px);
        if(it==cache.end()){
            if(cache.size()>=384)cache.clear();
            ThirdAgeHdPanel p;p.width_=int(std::ceil(advance+2*pad));p.height_=int(std::ceil(px*1.1+2*pad));
            const double baseline=pad+.75*px;p.pixels_.assign(size_t(p.width_)*p.height_,0);p.text(v,pad,baseline,px,0xffffffff);
            it=cache.emplace(key,Cached{p.width_,p.height_,advance,baseline,std::move(p.pixels_)}).first;
        }
        const auto& c=it->second;auto* t=SDL_CreateTexture(r,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,c.w,c.h);if(!t)return;
        if(SDL_UpdateTexture(t,nullptr,c.pixels.data(),c.w*4)==0){
            SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);SDL_SetTextureColorMod(t,color.r,color.g,color.b);SDL_SetTextureAlphaMod(t,color.a);
#if SDL_VERSION_ATLEAST(2,0,12)
            SDL_SetTextureScaleMode(t,SDL_ScaleModeLinear);
#endif
            const double shift=align==1?(box.w-c.advance/density)/2:align==2?box.w-c.advance/density:0;
            SDL_FRect dst{float(box.x+shift-pad/density),float(box.y+box.h/2.+font*.20-c.baseline/density),float(c.w/density),float(c.h/density)};
            const bool clipped=SDL_RenderIsClipEnabled(r);SDL_Rect old{},clip=box;SDL_RenderGetClipRect(r,&old);
            const bool visible=!clipped||SDL_IntersectRect(&old,&box,&clip);
            if(visible){SDL_RenderSetClipRect(r,&clip);SDL_RenderCopyF(r,t,nullptr,&dst);}
            SDL_RenderSetClipRect(r,clipped?&old:nullptr);
        }
        SDL_DestroyTexture(t);
    }
    void set_hd_portraits(bool enabled){
        if(hd_portraits_!=enabled){release();hd_portraits_=enabled;}
    }
    ThirdAgeHdPanel()=default;
    ThirdAgeHdPanel(const ThirdAgeHdPanel&)=delete;
    ~ThirdAgeHdPanel(){release();}
    void release(){if(card_)SDL_DestroyTexture(card_);if(underlay_)SDL_DestroyTexture(underlay_);if(highlight_)SDL_DestroyTexture(highlight_);card_=underlay_=highlight_=nullptr;highlight_key_.clear();}
    void preview(SDL_Renderer* r,const ThirdAgeUnitPanel& p,const SDL_Rect& dst,const ThirdAgePanelTheme& theme){
        float sx=1,sy=1;SDL_RenderGetScale(r,&sx,&sy);
        if(prepare(r,p,int(std::lround(dst.w*sx)),int(std::lround(dst.h*sy)),false,theme))SDL_RenderCopy(r,card_,nullptr,&dst);
    }
    void command_preview(SDL_Renderer* r,const ThirdAgeCommandPanel& p,const SDL_Rect& dst,const ThirdAgePanelTheme& theme){
        float sx=1,sy=1;SDL_RenderGetScale(r,&sx,&sy);
        if(prepare_command(r,p,int(std::lround(dst.w*sx)),int(std::lround(dst.h*sy)),theme))SDL_RenderCopy(r,card_,nullptr,&dst);
    }
    void draw_command(SDL_Renderer* r,const ThirdAgeCommandPanel& p,const SDL_Rect& game,const ThirdAgePanelTheme& theme){
        if(!p.valid||game.w<320||game.h<180)return;
        int lw=0,lh=0;SDL_RenderGetLogicalSize(r,&lw,&lh);if(lw||lh)return;
        const double s=game.h/1080.;const int w=std::max(1,int(std::lround(268*s))),h=std::max(1,int(std::lround(156*s))),margin=std::max(1,int(std::lround(12*s)));
        if(!prepare_command(r,p,w,h,theme))return;
        const SDL_Rect dst{game.x+margin,game.y+game.h-margin-h,w,h};SDL_RenderCopy(r,card_,nullptr,&dst);
    }
    void draw(SDL_Renderer* r,const ThirdAgeUnitPanel& p,const SDL_Rect& game,bool smooth=false,bool left=false,const ThirdAgePanelTheme& theme=ThirdAgePanelTheme{}){
        if(!p.valid||game.w<320||game.h<180)return;
        // Expanded view is in output-pixel coordinates. Never modify logical
        // size, viewport, target, clipping or scale here (input uses them too).
        int lw=0,lh=0;SDL_RenderGetLogicalSize(r,&lw,&lh);if(lw||lh)return;
        const double s=game.h/1080.;
        const int w=std::max(1,int(std::lround(440*s))),h=std::max(1,int(std::lround(112*s))),margin=std::max(1,int(std::lround(12*s)));
        if(!prepare(r,p,w,h,left,theme))return; // original remains intact on failure
        if(p.erase_native){
        if(!underlay_)underlay_=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,40,40);
        if(!underlay_||SDL_UpdateTexture(underlay_,nullptr,p.underlay.data(),120)!=0)return;
        SDL_SetTextureBlendMode(underlay_,SDL_BLENDMODE_NONE);
#if SDL_VERSION_ATLEAST(2,0,12)
        SDL_SetTextureScaleMode(underlay_,smooth?SDL_ScaleModeLinear:SDL_ScaleModeNearest);
        SDL_SetTextureScaleMode(card_,SDL_ScaleModeNearest);
#endif
        const SDL_FRect erased{float(game.x+game.w*280./320),float(game.y),float(game.w*40./320),float(game.h*40./180)};
        // Both textures are ready before replacing any original HUD pixels.
        if(SDL_RenderCopyF(r,underlay_,nullptr,&erased)!=0)return;
        }
        const SDL_Rect dst{left?game.x+margin:game.x+game.w-margin-w,game.y+margin,w,h};
        SDL_RenderCopy(r,card_,nullptr,&dst);
    }
};
}
