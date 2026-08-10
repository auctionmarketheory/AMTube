// AMTube - YouTube Lite for R36S
// Zero Dependency: stb_truetype + stb_image, no SDL2_ttf/SDL2_image

#include <SDL2/SDL.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef RES_PATH
#define RES_PATH "./res"
#endif

const int W = 640, H = 480;

// ── STB Font ─────────────────────────────────────────────────────────────────
#include <unordered_map>
class CustomFont {
public:
    stbtt_fontinfo info;
    std::vector<unsigned char> ttf_buffer;
    float scale;
    int ascent, descent, lineGap;
    float sz = 20.0f;
    
    struct Glyph {
        SDL_Texture* tex;
        int w, h, xoff, yoff, advance;
    };
    std::unordered_map<int, Glyph> cache;

    bool load(SDL_Renderer* r, const std::string& path, float s) {
        sz = s;
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f.is_open()) return false;
        auto len = f.tellg(); f.seekg(0);
        ttf_buffer.resize(len);
        f.read((char*)ttf_buffer.data(), len);
        
        if (!stbtt_InitFont(&info, ttf_buffer.data(), 0)) return false;
        
        scale = stbtt_ScaleForPixelHeight(&info, s);
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        return true;
    }

    Glyph getGlyph(SDL_Renderer* r, int cp) {
        if (cache.count(cp)) return cache[cp];
        Glyph g = {nullptr, 0, 0, 0, 0, 0};
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&info, cp, &adv, &lsb);
        g.advance = adv * scale;
        
        int x0,y0,x1,y1;
        stbtt_GetCodepointBitmapBox(&info, cp, scale, scale, &x0, &y0, &x1, &y1);
        g.w = x1 - x0;
        g.h = y1 - y0;
        g.xoff = x0;
        g.yoff = y0;
        
        if (g.w > 0 && g.h > 0) {
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&info, scale, scale, cp, &g.w, &g.h, &g.xoff, &g.yoff);
            std::vector<unsigned char> rgba(g.w * g.h * 4, 255);
            for (int i=0; i<g.w*g.h; i++) rgba[i*4+3] = bitmap[i];
            stbtt_FreeBitmap(bitmap, nullptr);
            SDL_Surface* sf = SDL_CreateRGBSurfaceFrom(rgba.data(), g.w, g.h, 32, g.w*4, 0xFF, 0xFF00, 0xFF0000, 0xFF000000);
            g.tex = SDL_CreateTextureFromSurface(r, sf);
            SDL_SetTextureBlendMode(g.tex, SDL_BLENDMODE_BLEND);
            SDL_FreeSurface(sf);
        }
        cache[cp] = g;
        return g;
    }

    uint32_t decodeUTF8(const std::string& str, size_t& i) {
        if (i >= str.length()) return 0;
        unsigned char c0 = str[i];
        if ((c0 & 0x80) == 0) { i += 1; return c0; }
        if ((c0 & 0xE0) == 0xC0) {
            if (i+1 >= str.length()) { i+=1; return 0; }
            unsigned char c1 = str[i+1];
            i += 2; return ((c0 & 0x1F) << 6) | (c1 & 0x3F);
        }
        if ((c0 & 0xF0) == 0xE0) {
            if (i+2 >= str.length()) { i+=1; return 0; }
            unsigned char c1 = str[i+1], c2 = str[i+2];
            i += 3; return ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
        }
        if ((c0 & 0xF8) == 0xF0) {
            if (i+3 >= str.length()) { i+=1; return 0; }
            unsigned char c1 = str[i+1], c2 = str[i+2], c3 = str[i+3];
            i += 4; return ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        }
        i += 1; return 0;
    }

    void draw(SDL_Renderer* r, float x, float y, const std::string& txt, SDL_Color c) {
        float cx = x;
        size_t i = 0;
        int base_y = y + ascent * scale;
        while(i < txt.length()) {
            uint32_t cp = decodeUTF8(txt, i);
            if (!cp) continue;
            Glyph g = getGlyph(r, cp);
            if (g.tex) {
                SDL_SetTextureColorMod(g.tex, c.r, c.g, c.b);
                SDL_SetTextureAlphaMod(g.tex, c.a);
                SDL_Rect d = { (int)(cx + g.xoff), (int)(base_y + g.yoff), g.w, g.h };
                SDL_RenderCopy(r, g.tex, nullptr, &d);
            }
            cx += g.advance;
        }
    }

    void drawWrap(SDL_Renderer* r, float x, float y, const std::string& txt, SDL_Color c, int maxW) {
        float cx = x;
        float cy = y;
        int base_y = cy + ascent * scale;
        
        std::istringstream iss(txt);
        std::string word;
        while (std::getline(iss, word, ' ')) {
            float ww = 0;
            size_t wi = 0;
            while(wi < word.length()) {
                uint32_t cp = decodeUTF8(word, wi);
                if (cp) ww += getGlyph(r, cp).advance;
            }
            
            Glyph spaceG = getGlyph(r, 32);
            if (cx > x && (cx - x + spaceG.advance + ww) > maxW) {
                cy += (ascent - descent + lineGap) * scale;
                base_y = cy + ascent * scale;
                cx = x;
            } else if (cx > x) {
                cx += spaceG.advance;
            }
            
            wi = 0;
            while(wi < word.length()) {
                uint32_t cp = decodeUTF8(word, wi);
                if (!cp) continue;
                Glyph g = getGlyph(r, cp);
                if (g.tex) {
                    SDL_SetTextureColorMod(g.tex, c.r, c.g, c.b);
                    SDL_SetTextureAlphaMod(g.tex, c.a);
                    SDL_Rect d = { (int)(cx + g.xoff), (int)(base_y + g.yoff), g.w, g.h };
                    SDL_RenderCopy(r, g.tex, nullptr, &d);
                }
                cx += g.advance;
            }
        }
    }

    ~CustomFont() {
        for(auto& pair : cache) if(pair.second.tex) SDL_DestroyTexture(pair.second.tex);
    }
};

// ── Data ──────────────────────────────────────────────────────────────────────
struct Video { std::string id,title,author,thumb; SDL_Texture* tex=nullptr; };
enum State { LIST, MENU, PLAYING };

SDL_Window*   win  = nullptr;
SDL_Renderer* ren  = nullptr;
CustomFont    fBig, fSm;
std::vector<Video> vids;
int sel=0; bool run=true;
std::string cat="Kênh Đăng Ký";
State st=LIST;
std::vector<std::string> menu={"Kênh Đăng Ký","Thịnh Hành","Nhạc Lofi","Trò Chơi","Tin Tức","Thoát"};
int mi=0;

// ── Helpers ───────────────────────────────────────────────────────────────────
SDL_Texture* loadImg(const std::string& p) {
    int w,h,ch; unsigned char* d=stbi_load(p.c_str(),&w,&h,&ch,4);
    if(!d){std::cerr<<"[ERR] img "<<p<<": "<<stbi_failure_reason()<<std::endl;return nullptr;}
    SDL_Surface* sf=SDL_CreateRGBSurfaceWithFormatFrom(d,w,h,32,w*4,SDL_PIXELFORMAT_RGBA32);
    SDL_Texture* t=sf?SDL_CreateTextureFromSurface(ren,sf):nullptr;
    if(sf)SDL_FreeSurface(sf); stbi_image_free(d); return t;
}

void loadData() {
    for(auto&v:vids)if(v.tex)SDL_DestroyTexture(v.tex);
    vids.clear(); sel=0;
    std::ifstream f("/tmp/yt_data/yt_data.txt");
    if(!f.is_open()){std::cerr<<"[ERR] no yt_data.txt"<<std::endl;return;}
    std::string ln;
    while(std::getline(f,ln)){
        std::stringstream ss(ln); std::string it; Video v; int c=0;
        while(std::getline(ss,it,'|')){if(c==0)v.id=it;else if(c==1)v.title=it;else if(c==2)v.author=it;else if(c==3)v.thumb=it;c++;}
        if(!v.thumb.empty())v.tex=loadImg(v.thumb);
        vids.push_back(v);
    }
    std::cerr<<"[OK] loaded "<<vids.size()<<" videos"<<std::endl;
}

void backend(bool reload) {
    SDL_SetRenderDrawColor(ren,20,20,20,255); SDL_RenderClear(ren);
    fBig.draw(ren,120,230,"Đang chuyển kênh... Đang tải dữ liệu...",{255,255,255,255});
    SDL_RenderPresent(ren);
    std::string m=cat; 
    if(m=="Kênh Đăng Ký") m="Subscribed";
    else if(m=="Thịnh Hành") m="Trending";
    else if(m=="Nhạc Lofi") m="Lofi Music";
    else if(m=="Trò Chơi") m="Gaming";
    else if(m=="Tin Tức") m="News";
    
    std::string cmd="./amtube_backend.sh --category \""+m+"\"";
    if(reload)cmd+=" --reload";
    std::cerr<<"[CMD] "<<cmd<<std::endl;
    system(cmd.c_str());
    loadData();
}

void drawMenu() {
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,0,0,0,180);
    SDL_Rect full={0,0,W,H}; SDL_RenderFillRect(ren,&full);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    int bw=300,bh=260,bx=(W-bw)/2,by=(H-bh)/2;
    SDL_Rect box={bx,by,bw,bh};
    SDL_SetRenderDrawColor(ren,40,40,40,255); SDL_RenderFillRect(ren,&box);
    SDL_SetRenderDrawColor(ren,255,255,255,255); SDL_RenderDrawRect(ren,&box);
    fBig.draw(ren,bx+50,by+20,"--- THỂ LOẠI ---",{255,200,0,255});
    for(int i=0;i<(int)menu.size();i++){
        int iy=by+52+i*30;
        if(i==mi){SDL_Rect h={bx+10,iy-2,bw-20,28};SDL_SetRenderDrawColor(ren,100,100,100,255);SDL_RenderFillRect(ren,&h);}
        SDL_Color c=(menu[i]=="Thoát")?SDL_Color{255,50,50,255}:SDL_Color{255,255,255,255};
        fBig.draw(ren,bx+20,iy+16,menu[i],c);
    }
}

void drawFrame() {
    SDL_SetRenderDrawColor(ren,20,20,20,255); SDL_RenderClear(ren);
    SDL_Rect hdr={0,0,W,40}; SDL_SetRenderDrawColor(ren,30,30,30,255); SDL_RenderFillRect(ren,&hdr);
    fBig.draw(ren,10,28,"AMTube - "+cat,{255,255,255,255});
    fSm.draw(ren,340,26,"Y:DANH MỤC X:TẢI LẠI A:PHÁT",{180,180,180,255});
    int sy=50,ih=90,si=sel-1; if(si<0)si=0;
    for(int i=si;i<si+4&&i<(int)vids.size();i++){
        int dy=sy+(i-si)*ih;
        if(i==sel){SDL_SetRenderDrawColor(ren,220,30,30,255);SDL_Rect r={4,dy,W-8,ih-5};SDL_RenderDrawRect(ren,&r);}
        auto&v=vids[i];
        if(v.tex){SDL_Rect r={10,dy+5,128,72};SDL_RenderCopy(ren,v.tex,nullptr,&r);}
        else{SDL_Rect r={10,dy+5,128,72};SDL_SetRenderDrawColor(ren,50,50,50,255);SDL_RenderFillRect(ren,&r);}
        fBig.drawWrap(ren,150,dy+20,v.title,{255,255,255,255},470);
        fSm.draw(ren,150,dy+60,v.author,{150,150,150,255});
    }
    if(st==MENU) drawMenu();
    else if(st==PLAYING){
        SDL_SetRenderDrawColor(ren,0,0,0,255); SDL_RenderClear(ren);
        fBig.draw(ren,150,230,"Đang phát Video... Bấm B để dừng.",{255,255,255,255});
    }
    SDL_RenderPresent(ren);
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* args[]) {
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK)<0){std::cerr<<"SDL_Init: "<<SDL_GetError()<<std::endl;return 1;}
    if(SDL_NumJoysticks()>0)SDL_JoystickOpen(0);
    win=SDL_CreateWindow("AMTube",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,W,H,SDL_WINDOW_SHOWN);
    ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!win||!ren){std::cerr<<"Window/Renderer: "<<SDL_GetError()<<std::endl;return 1;}
    std::string fp = "../arial.ttf";
    if(!fBig.load(ren,fp,20.0f)||!fSm.load(ren,fp,16.0f)){
        std::cerr<<"[WARN] ../arial.ttf not found, fallback to ./font.ttf"<<std::endl;
        fp = "./font.ttf";
        if(!fBig.load(ren,fp,20.0f)||!fSm.load(ren,fp,16.0f)){
            std::cerr<<"[ERR] Font failed: "<<fp<<std::endl; return 1;
        }
    }
    std::cerr<<"[OK] Init done"<<std::endl;

    backend(false);

    SDL_Event e; int prevY=0;
    while(run){
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT){ run=false; }

            else if(e.type==SDL_KEYDOWN){
                if(e.key.keysym.sym==SDLK_ESCAPE) run=false;
                else if(e.key.keysym.sym==SDLK_UP){
                    if(st==LIST&&sel>0)sel--;
                    else if(st==MENU&&mi>0)mi--;
                }
                else if(e.key.keysym.sym==SDLK_DOWN){
                    if(st==LIST&&sel<(int)vids.size()-1)sel++;
                    else if(st==MENU&&mi<(int)menu.size()-1)mi++;
                }
            }

            else if(e.type==SDL_JOYBUTTONDOWN){
                int b = (int)e.jbutton.button;
                // R36S Clone: A=0,B=1,X=2,Y=3 | DPad: UP=13,DOWN=14,LEFT=15,RIGHT=16
                bool up   = (b==13);
                bool down = (b==14);

                if (up) {
                    if(st==LIST&&sel>0)sel--;
                    else if(st==MENU&&mi>0)mi--;
                } else if (down) {
                    if(st==LIST&&sel<(int)vids.size()-1)sel++;
                    else if(st==MENU&&mi<(int)menu.size()-1)mi++;
                } else {
                    switch(b){
                        case 0: // A - Confirm/Play
                            if(st==MENU){
                                if(menu[mi]=="Thoát"){run=false;}
                                else{cat=menu[mi];st=LIST;backend(false);}
                            } else if(st==LIST&&!vids.empty()){
                                st=PLAYING;
                                for(auto&v:vids)if(v.tex){SDL_DestroyTexture(v.tex);v.tex=nullptr;}
                                SDL_SetRenderDrawColor(ren,0,0,0,255); SDL_RenderClear(ren);
                                fBig.draw(ren,150,230,"Đang lấy luồng Video... Vui lòng đợi!",{255,255,255,255});
                                SDL_RenderPresent(ren);

                                std::string fetch_cmd = "if [ -f ./yt-dlp ]; then ./yt-dlp --no-check-certificate -g -f \"bestvideo[height<=?480]+bestaudio/best\" \"https://youtube.com/watch?v="+vids[sel].id+"\"; else ./youtube-dl --no-check-certificate -g -f \"bestvideo[height<=?480]+bestaudio/best\" \"https://youtube.com/watch?v="+vids[sel].id+"\"; fi";
                                FILE* pipe = popen(fetch_cmd.c_str(), "r");
                                std::string raw_url = "";
                                if (pipe) {
                                    char buffer[128];
                                    while (fgets(buffer, 128, pipe) != NULL) raw_url += buffer;
                                    pclose(pipe);
                                }
                                
                                if (!raw_url.empty()) {
                                    raw_url.erase(raw_url.find_last_not_of(" \n\r\t")+1);
                                    size_t nl = raw_url.find('\n');
                                    std::string vid_url = (nl != std::string::npos) ? raw_url.substr(0, nl) : raw_url;
                                    std::string aud_url = (nl != std::string::npos) ? raw_url.substr(nl+1) : "";
                                    
                                    std::string cmd="mpv --fs '"+vid_url+"'";
                                    if(!aud_url.empty()) cmd += " --audio-file='"+aud_url+"'";
                                    cmd += " &";
                                    system(cmd.c_str());
                                } else {
                                    st=LIST;
                                    backend(false);
                                }
                            }
                            break;
                        case 1: // B - Back
                            if(st==MENU){st=LIST;}
                            else if(st==PLAYING){system("killall -9 mpv");st=LIST;loadData();}
                            break;
                        case 2: // X - Reload/Zap
                            if(st==LIST)backend(true);
                            break;
                        case 3: // Y - Menu
                            if(st==LIST)st=MENU;
                            break;
                    }
                }
            }

            else if(e.type==SDL_JOYHATMOTION){
                std::cerr<<"[HAT] hat="<<(int)e.jhat.hat<<" val="<<(int)e.jhat.value<<std::endl;
                if(e.jhat.value==SDL_HAT_UP){
                    if(st==LIST&&sel>0)sel--;
                    else if(st==MENU&&mi>0)mi--;
                } else if(e.jhat.value==SDL_HAT_DOWN){
                    if(st==LIST&&sel<(int)vids.size()-1)sel++;
                    else if(st==MENU&&mi<(int)menu.size()-1)mi++;
                }
            }

            else if(e.type==SDL_JOYAXISMOTION){
                std::cerr<<"[AXIS] axis="<<(int)e.jaxis.axis<<" val="<<e.jaxis.value<<std::endl;
                if(e.jaxis.axis==1){
                    int v=e.jaxis.value;
                    if(v<-8000&&prevY>=-8000){
                        if(st==LIST&&sel>0)sel--;
                        else if(st==MENU&&mi>0)mi--;
                    } else if(v>8000&&prevY<=8000){
                        if(st==LIST&&sel<(int)vids.size()-1)sel++;
                        else if(st==MENU&&mi<(int)menu.size()-1)mi++;
                    }
                    prevY=v;
                }
            }
        }
        if(st!=PLAYING)drawFrame();
        SDL_Delay(16);
    }
    for(auto&v:vids)if(v.tex)SDL_DestroyTexture(v.tex);
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
