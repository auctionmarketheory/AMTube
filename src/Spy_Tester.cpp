#include <SDL.h>
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "def.h"

SDL_Window* g_window = NULL;
SDL_Renderer* g_renderer = NULL;
SDL_Joystick* g_joystick = NULL;

class CustomFont {
public:
    SDL_Texture* atlas;
    stbtt_bakedchar cdata[96];
    int tex_w, tex_h;
    float size;

    CustomFont() : atlas(NULL), tex_w(512), tex_h(512), size(20.0f) {}

    bool load(SDL_Renderer* renderer, const std::string& path, float font_size) {
        size = font_size;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if(!file.is_open()) return false;
        std::streamsize size_file = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<unsigned char> buffer(size_file);
        if(!file.read((char*)buffer.data(), size_file)) return false;

        std::vector<unsigned char> bitmap(tex_w * tex_h);
        stbtt_BakeFontBitmap(buffer.data(), 0, size, bitmap.data(), tex_w, tex_h, 32, 96, cdata);

        std::vector<unsigned char> rgba(tex_w * tex_h * 4, 255);
        for(int i=0; i<tex_w*tex_h; i++) {
            rgba[i*4 + 3] = bitmap[i];
        }

        SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(rgba.data(), tex_w, tex_h, 32, tex_w * 4,
            0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
        if(!surface) return false;

        atlas = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        
        SDL_SetTextureBlendMode(atlas, SDL_BLENDMODE_BLEND);
        return atlas != NULL;
    }

    void renderText(SDL_Renderer* renderer, float x, float y, const std::string& text, SDL_Color color) {
        SDL_SetTextureColorMod(atlas, color.r, color.g, color.b);
        for (char c : text) {
            if (c >= 32 && c < 128) {
                stbtt_aligned_quad q;
                stbtt_GetBakedQuad(cdata, tex_w, tex_h, c - 32, &x, &y, &q, 1);
                
                SDL_Rect src = { (int)(q.s0 * tex_w), (int)(q.t0 * tex_h), 
                                 (int)((q.s1 - q.s0) * tex_w), (int)((q.t1 - q.t0) * tex_h) };
                SDL_Rect dst = { (int)q.x0, (int)q.y0, (int)(q.x1 - q.x0), (int)(q.y1 - q.y0) };
                SDL_RenderCopy(renderer, atlas, &src, &dst);
            }
        }
    }

    ~CustomFont() {
        if(atlas) SDL_DestroyTexture(atlas);
    }
};

std::atomic<bool> is_spying(false);
std::atomic<bool> spy_finished(false);
std::atomic<bool> thread_running(false);
std::atomic<int> scan_progress(0);

void spy_thread() {
    thread_running = true;
    system("rm -f /tmp/spy_log.txt");
    system("bash ./Spy_Tool_Backend.sh > /tmp/spy_log.txt 2>&1");
    spy_finished = true;
    is_spying = false;
    thread_running = false;
}

std::vector<std::string> tail_log(const std::string& path, int lines) {
    std::vector<std::string> out;
    std::ifstream file(path.c_str());
    if(!file.is_open()) return out;
    std::string line;
    std::vector<std::string> all_lines;
    while(std::getline(file, line)) {
        all_lines.push_back(line);
    }
    int start = all_lines.size() > (size_t)lines ? all_lines.size() - lines : 0;
    for(size_t i = start; i < all_lines.size(); ++i) {
        out.push_back(all_lines[i]);
    }
    return out;
}

bool init_sdl() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) return false;
    if (SDL_NumJoysticks() >= 1) {
        g_joystick = SDL_JoystickOpen(0);
    }
    #if FULLSCREEN == 1
       g_window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    #else
       g_window = SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    #endif
    if (!g_window) return false;
    #if HARDWARE_ACCELERATION == 1
       g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    #else
       g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    #endif
    if (!g_renderer) return false;
    return true;
}

void DrawGlowLine(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, SDL_Color color) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 40);
    SDL_RenderDrawLine(renderer, x1-2, y1-2, x2-2, y2-2);
    SDL_RenderDrawLine(renderer, x1+2, y1+2, x2+2, y2+2);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 100);
    SDL_RenderDrawLine(renderer, x1-1, y1-1, x2-1, y2-1);
    SDL_RenderDrawLine(renderer, x1+1, y1+1, x2+1, y2+1);
    SDL_SetRenderDrawColor(renderer, color.r + 50 > 255 ? 255 : color.r + 50, 
                                     color.g + 50 > 255 ? 255 : color.g + 50, 
                                     color.b + 50 > 255 ? 255 : color.b + 50, 255);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void DrawHyperHUD(SDL_Renderer* renderer, SDL_Color cyan, SDL_Color pink, SDL_Color orange) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, cyan.r, cyan.g, cyan.b, 20);
    // Background Grid
    for(int i=0; i<SCREEN_WIDTH; i+=30) SDL_RenderDrawLine(renderer, i, 0, i, SCREEN_HEIGHT);
    for(int i=0; i<SCREEN_HEIGHT; i+=30) SDL_RenderDrawLine(renderer, 0, i, SCREEN_WIDTH, i);

    int pad = 10;
    int chamfer = 25;
    int w = SCREEN_WIDTH - pad*2;
    int h = SCREEN_HEIGHT - pad*2;
    int split_x = pad + w - 180; // Cột phải to hơn chút

    // Khung viền ngoài
    DrawGlowLine(renderer, pad + chamfer, pad, pad + w - chamfer, pad, cyan); 
    DrawGlowLine(renderer, pad + w - chamfer, pad, pad + w, pad + chamfer, cyan); 
    DrawGlowLine(renderer, pad + w, pad + chamfer, pad + w, pad + h - chamfer, cyan); 
    DrawGlowLine(renderer, pad + w, pad + h - chamfer, pad + w - chamfer, pad + h, cyan); 
    DrawGlowLine(renderer, pad + w - chamfer, pad + h, pad + chamfer, pad + h, cyan); 
    DrawGlowLine(renderer, pad + chamfer, pad + h, pad, pad + h - chamfer, cyan); 
    DrawGlowLine(renderer, pad, pad + h - chamfer, pad, pad + chamfer, cyan); 
    DrawGlowLine(renderer, pad, pad + chamfer, pad + chamfer, pad, cyan); 

    // Header Line
    DrawGlowLine(renderer, pad, pad + 35, pad + w, pad + 35, pink);
    
    // Split cột phải
    DrawGlowLine(renderer, split_x, pad + 35, split_x, pad + h, cyan);

    // Split phụ ở cột phải (chia thành 2 khối vuông nhỏ)
    DrawGlowLine(renderer, split_x, pad + 150, pad + w, pad + 150, orange);
    DrawGlowLine(renderer, split_x, pad + 300, pad + w, pad + 300, orange);
}

void DrawScanlines(SDL_Renderer* renderer) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 100); 
    for (int y = 0; y < SCREEN_HEIGHT; y += 3) {
        SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);
    }
}

std::string generateHex(int length) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');
    for(int i=0; i<length; i++) {
        ss << std::setw(2) << (rand() % 0xFF);
    }
    return ss.str();
}

int main(int argc, char* args[]) {
    if (!init_sdl()) return 1;

    CustomFont font, fontMono, fontSmall;
    if (!font.load(g_renderer, std::string(RES_PATH) + "/" + FONT_NAME, FONT_SIZE + 4)) return 1;
    if (!fontMono.load(g_renderer, std::string(RES_PATH) + "/" + FONT_NAME_MONO, FONT_SIZE)) return 1;
    if (!fontSmall.load(g_renderer, std::string(RES_PATH) + "/" + FONT_NAME_MONO, FONT_SIZE - 4)) return 1;

    SDL_Color neonCyan = {0, 255, 255, 255};
    SDL_Color hotPink = {255, 0, 128, 255};
    SDL_Color goldenOrange = {255, 165, 0, 255};
    SDL_Color neonGreen = {0, 255, 50, 255};
    SDL_Color redError = {255, 50, 50, 255};
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color bgDark = {8, 12, 18, 255};

    bool quit = false;
    bool show_menu = false;
    int menu_selection = 0; 
    
    std::vector<std::string> telemetry;
    for(int i=0; i<15; i++) telemetry.push_back(generateHex(8));

    system("echo '> SYSTEM STARTUP [NEO-TERMINAL v7.8]' > /tmp/spy_log.txt");
    system("echo '> AWAITING INFILTRATION COMMAND...' >> /tmp/spy_log.txt");

    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) quit = true;
            bool pressed_up = false;
            bool pressed_down = false;
            bool pressed_menu = false;
            bool pressed_validate = false;
            bool pressed_back = false;

            if (event.type == SDL_JOYBUTTONDOWN) {
                int b = event.jbutton.button;
                if (b == 0 || b == 1) pressed_validate = true;
                if (b == 1 || b == 0) pressed_back = true; 
                if (b == 2 || b == 3) pressed_menu = true; 
                if (b == 6 || b == 13) pressed_up = true; 
                if (b == 7 || b == 14) pressed_down = true;
            } else if (event.type == SDL_JOYHATMOTION) {
                if (event.jhat.value & SDL_HAT_UP) pressed_up = true;
                if (event.jhat.value & SDL_HAT_DOWN) pressed_down = true;
            } else if (event.type == SDL_JOYAXISMOTION) {
                if (event.jaxis.axis == 1) { 
                    if (event.jaxis.value < -16000) pressed_up = true;
                    if (event.jaxis.value > 16000) pressed_down = true;
                }
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_UP) pressed_up = true;
                if (event.key.keysym.sym == SDLK_DOWN) pressed_down = true;
                if (event.key.keysym.sym == SDLK_RETURN) pressed_validate = true;
                if (event.key.keysym.sym == SDLK_ESCAPE) pressed_back = true;
                if (event.key.keysym.sym == SDLK_m || event.key.keysym.sym == SDLK_x) pressed_menu = true;
            }

            if (pressed_menu) {
                show_menu = !show_menu;
                menu_selection = 0;
            }
            else if (show_menu) {
                if (pressed_up) menu_selection = (menu_selection == 1) ? 0 : 1;
                if (pressed_down) menu_selection = (menu_selection == 0) ? 1 : 0;
                
                if (pressed_validate) { 
                    if (menu_selection == 1) quit = true; 
                    if (menu_selection == 0) {
                        // Restart log
                        system("echo '> AWAITING INFILTRATION COMMAND...' > /tmp/spy_log.txt");
                        spy_finished = false;
                        scan_progress = 0;
                    }
                    show_menu = false;
                }
            }
            else {
                if (pressed_validate && !thread_running && !spy_finished) { 
                    is_spying = true;
                    scan_progress = 0;
                    std::thread t(spy_thread);
                    t.detach();
                }
            }
        }

        if(rand() % 3 == 0) {
            telemetry.erase(telemetry.begin());
            telemetry.push_back(generateHex(8));
        }
        
        if (is_spying && scan_progress < 100) {
            scan_progress += (rand() % 3);
            if (scan_progress > 99) scan_progress = 99;
        } else if (spy_finished) {
            scan_progress = 100;
        }

        SDL_SetRenderDrawColor(g_renderer, bgDark.r, bgDark.g, bgDark.b, 255);
        SDL_RenderClear(g_renderer);

        DrawHyperHUD(g_renderer, neonCyan, hotPink, goldenOrange);

        font.renderText(g_renderer, 30, 32, ">>> SYSTEM INFILTRATION CORE", neonCyan);
        
        std::string clock_str = "SYS.CLOCK: " + std::to_string(SDL_GetTicks() % 9999);
        fontSmall.renderText(g_renderer, SCREEN_WIDTH - 170, 32, clock_str, goldenOrange);

        // Main Log Area
        std::vector<std::string> logs = tail_log("/tmp/spy_log.txt", (SCREEN_HEIGHT - 130)/LINE_HEIGHT);
        int y = 60;
        for (size_t i = 0; i < logs.size(); ++i) {
            SDL_Color logColor = neonGreen;
            if (logs[i].find("SUCCESS") != std::string::npos || logs[i].find("ACTIVE") != std::string::npos) logColor = hotPink;
            if (logs[i].find("WARNING") != std::string::npos || logs[i].find("BREACH") != std::string::npos) logColor = redError;
            if (logs[i].find("Wait") != std::string::npos) logColor = goldenOrange;
            
            fontMono.renderText(g_renderer, 25, y, logs[i], logColor);
            
            if (i == logs.size() - 1 && is_spying) {
                if ((SDL_GetTicks() / 200) % 2 == 0) {
                    fontMono.renderText(g_renderer, 25 + logs[i].length() * 10, y, "\xDB", hotPink); 
                }
            }
            y += LINE_HEIGHT;
        }

        // Fake Data Column 1
        int tel_y = 60;
        fontSmall.renderText(g_renderer, SCREEN_WIDTH - 160, tel_y, "[DATA STREAM]", hotPink);
        tel_y += 20;
        for(size_t i=0; i<6; i++) {
            fontSmall.renderText(g_renderer, SCREEN_WIDTH - 160, tel_y, telemetry[i], neonCyan);
            tel_y += 18;
        }

        // Fake Data Column 2 (Modules)
        tel_y = 180;
        fontSmall.renderText(g_renderer, SCREEN_WIDTH - 160, tel_y, "[MODULES]", goldenOrange);
        tel_y += 20;
        fontSmall.renderText(g_renderer, SCREEN_WIDTH - 160, tel_y, "DECRYPT_X7: ON", white); tel_y+=18;
        fontSmall.renderText(g_renderer, SCREEN_WIDTH - 160, tel_y, "BYPASS_V2 : ON", white); tel_y+=18;
        fontSmall.renderText(g_renderer, SCREEN_WIDTH - 160, tel_y, "NET_SNIFF : ACT", white); tel_y+=18;
        fontSmall.renderText(g_renderer, SCREEN_WIDTH - 160, tel_y, "SYS_DUMP  : RDY", white); tel_y+=18;
        
        // Progress Bar Area
        tel_y = 330;
        fontSmall.renderText(g_renderer, SCREEN_WIDTH - 160, tel_y, "[INFILTRATION]", neonCyan);
        tel_y += 20;
        std::string prog_text = std::to_string((int)scan_progress) + "% COMPLETED";
        fontSmall.renderText(g_renderer, SCREEN_WIDTH - 160, tel_y, prog_text, hotPink);
        tel_y += 20;
        
        // Vẽ vạch progress
        SDL_Rect pbar_bg = { SCREEN_WIDTH - 160, tel_y, 140, 15 };
        SDL_SetRenderDrawColor(g_renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(g_renderer, &pbar_bg);
        
        SDL_Rect pbar_fg = { SCREEN_WIDTH - 160, tel_y, (int)(140 * scan_progress / 100), 15 };
        SDL_SetRenderDrawColor(g_renderer, hotPink.r, hotPink.g, hotPink.b, 255);
        SDL_RenderFillRect(g_renderer, &pbar_fg);


        if (!show_menu) {
            std::string status = is_spying ? "[HACKING... DO NOT ABORT]" : (spy_finished ? "[BREACH SUCCESSFUL]" : "[SYSTEM IDLE]");
            SDL_Color statColor = is_spying ? goldenOrange : (spy_finished ? neonGreen : neonCyan);
            font.renderText(g_renderer, 30, SCREEN_HEIGHT - 22, status, statColor);
            
            if (!is_spying && !spy_finished) {
                font.renderText(g_renderer, SCREEN_WIDTH/2 - 60, SCREEN_HEIGHT - 22, "[A] INFILTRATE", white);
            }
            font.renderText(g_renderer, SCREEN_WIDTH - 160, SCREEN_HEIGHT - 22, "[X] SYS MENU", white);
        } else {
            SDL_Rect menuRect = { 20, SCREEN_HEIGHT - 70, SCREEN_WIDTH - 100, 50 };
            SDL_SetRenderDrawColor(g_renderer, bgDark.r, bgDark.g, bgDark.b, 240);
            SDL_RenderFillRect(g_renderer, &menuRect);
            DrawGlowLine(g_renderer, menuRect.x, menuRect.y, menuRect.x+menuRect.w, menuRect.y, hotPink);

            font.renderText(g_renderer, 30, SCREEN_HEIGHT - 35, "SYS_CONTROL:", goldenOrange);
            
            for (int i=0; i<2; i++) {
                int item_x = 220 + i * 180;
                std::string text = (i == 0) ? "[1] RESET LOG" : "[2] SHUTDOWN";
                
                if (menu_selection == i) {
                    SDL_Rect hl = { item_x - 5, SCREEN_HEIGHT - 55, 150, 30 };
                    SDL_SetRenderDrawColor(g_renderer, hotPink.r, hotPink.g, hotPink.b, 255);
                    SDL_RenderFillRect(g_renderer, &hl);
                    font.renderText(g_renderer, item_x, SCREEN_HEIGHT - 35, text, bgDark); 
                } else {
                    font.renderText(g_renderer, item_x, SCREEN_HEIGHT - 35, text, neonCyan);
                }
            }
        }

        DrawScanlines(g_renderer);
        SDL_RenderPresent(g_renderer);
        SDL_Delay(MS_PER_FRAME);
    }
    
    if (g_joystick) SDL_JoystickClose(g_joystick);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    SDL_Quit();
    return 0;
}
