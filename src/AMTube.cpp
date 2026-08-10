// AMTube - YouTube Lite for R36S
// Architecture: Zero Dependency (SDL2 core only, stb headers embedded)
// Font: stb_truetype.h | Image: stb_image.h
// No SDL2_ttf, No SDL2_image

#include <SDL2/SDL.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cmath>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef RES_PATH
#define RES_PATH "./res"
#endif

// --- Constants ---
const int SCREEN_WIDTH  = 640;
const int SCREEN_HEIGHT = 480;

// --- STB Font Wrapper (inherited from WiFi_Tester architecture) ---
class CustomFont {
public:
    SDL_Texture* atlas;
    stbtt_bakedchar cdata[96]; // ASCII 32..127
    int tex_w, tex_h;
    float size;

    CustomFont() : atlas(NULL), tex_w(512), tex_h(512), size(20.0f) {}

    bool load(SDL_Renderer* renderer, const std::string& path, float font_size) {
        size = font_size;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "[C++ ERROR] Cannot open font: " << path << std::endl;
            return false;
        }
        std::streamsize sz = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<unsigned char> buffer(sz);
        if (!file.read((char*)buffer.data(), sz)) return false;

        std::vector<unsigned char> bitmap(tex_w * tex_h);
        stbtt_BakeFontBitmap(buffer.data(), 0, font_size, bitmap.data(), tex_w, tex_h, 32, 96, cdata);

        // Convert grayscale bitmap to RGBA
        std::vector<unsigned char> rgba(tex_w * tex_h * 4, 255);
        for (int i = 0; i < tex_w * tex_h; i++) {
            rgba[i * 4 + 3] = bitmap[i];
        }

        SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(rgba.data(), tex_w, tex_h, 32, tex_w * 4,
            0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
        if (!surface) return false;

        atlas = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        SDL_SetTextureBlendMode(atlas, SDL_BLENDMODE_BLEND);
        std::cerr << "[C++ DEBUG] Font loaded: " << path << " size=" << font_size << std::endl;
        return atlas != NULL;
    }

    // Render ASCII text (tiêu đề Latin không dấu)
    void renderText(SDL_Renderer* renderer, float x, float y, const std::string& text, SDL_Color color) {
        if (!atlas) return;
        SDL_SetTextureColorMod(atlas, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(atlas, color.a);
        for (unsigned char c : text) {
            if (c >= 32 && c < 128) {
                stbtt_aligned_quad q;
                stbtt_GetBakedQuad(cdata, tex_w, tex_h, c - 32, &x, &y, &q, 1);
                SDL_Rect src = { (int)(q.s0 * tex_w), (int)(q.t0 * tex_h),
                                 (int)((q.s1 - q.s0) * tex_w), (int)((q.t1 - q.t0) * tex_h) };
                SDL_Rect dst = { (int)q.x0, (int)q.y0,
                                 (int)(q.x1 - q.x0), (int)(q.y1 - q.y0) };
                SDL_RenderCopy(renderer, atlas, &src, &dst);
            }
            // Non-ASCII (UTF-8 multi-byte) -> skip gracefully; only box-draw
        }
    }

    // Render text with max-width wrapping (returns final Y after wrapping)
    int renderTextWrapped(SDL_Renderer* renderer, float x, float baseY, const std::string& text,
                          SDL_Color color, int maxWidth) {
        if (!atlas) return (int)baseY;
        SDL_SetTextureColorMod(atlas, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(atlas, color.a);

        float lineX = x;
        float lineY = baseY;
        float startX = x;
        float lineH = size + 2.0f;

        // Tokenize by words for wrapping
        std::istringstream iss(text);
        std::string word;
        bool firstWord = true;

        while (std::getline(iss, word, ' ')) {
            // Measure word width
            float wordW = 0.0f;
            float dummyX = 0.0f, dummyY = 0.0f;
            for (unsigned char c : word) {
                if (c >= 32 && c < 128) {
                    stbtt_aligned_quad q;
                    stbtt_GetBakedQuad(cdata, tex_w, tex_h, c - 32, &dummyX, &dummyY, &q, 1);
                }
            }
            wordW = dummyX;

            // Measure space
            float spaceW = 0.0f;
            if (!firstWord) {
                float sx = 0.0f, sy = 0.0f;
                if (' ' >= 32 && ' ' < 128) {
                    stbtt_aligned_quad sq;
                    stbtt_GetBakedQuad(cdata, tex_w, tex_h, ' ' - 32, &sx, &sy, &sq, 1);
                    spaceW = sx;
                }
            }

            if (!firstWord && (lineX - startX + spaceW + wordW) > maxWidth) {
                // Wrap
                lineY += lineH;
                lineX = startX;
            } else if (!firstWord) {
                // Add space
                if (' ' >= 32 && ' ' < 128) {
                    stbtt_aligned_quad sq;
                    stbtt_GetBakedQuad(cdata, tex_w, tex_h, ' ' - 32, &lineX, &lineY, &sq, 1);
                }
            }

            // Render word chars
            for (unsigned char c : word) {
                if (c >= 32 && c < 128) {
                    stbtt_aligned_quad q;
                    stbtt_GetBakedQuad(cdata, tex_w, tex_h, c - 32, &lineX, &lineY, &q, 1);
                    SDL_Rect src = { (int)(q.s0 * tex_w), (int)(q.t0 * tex_h),
                                     (int)((q.s1 - q.s0) * tex_w), (int)((q.t1 - q.t0) * tex_h) };
                    SDL_Rect dst = { (int)q.x0, (int)q.y0,
                                     (int)(q.x1 - q.x0), (int)(q.y1 - q.y0) };
                    SDL_RenderCopy(renderer, atlas, &src, &dst);
                }
            }
            firstWord = false;
        }
        return (int)(lineY + lineH);
    }

    ~CustomFont() {
        if (atlas) SDL_DestroyTexture(atlas);
    }
};

// --- Data ---
struct YouTubeVideo {
    std::string id;
    std::string title;
    std::string author;
    std::string local_thumb;
    SDL_Texture* texture = nullptr;
};

enum AppState {
    VIEW_LIST,
    VIEW_MENU,
    PLAYING_VIDEO
};

// --- Globals ---
SDL_Window*   window   = nullptr;
SDL_Renderer* renderer = nullptr;
CustomFont    fontTitle;
CustomFont    fontAuthor;

std::vector<YouTubeVideo> videoList;
int selectedIndex = 0;
bool isRunning = true;

std::string currentCategory = "Subscribed";
AppState state = VIEW_LIST;

std::vector<std::string> menuItems = {
    "Kenh Dang Ky",
    "Trending",
    "Lofi Music",
    "Gaming",
    "News",
    "Exit"
};
int menuIndex = 0;

// --- Prototypes ---
void initSDL();
void cleanup();
void loadData();
void drawList();
void drawMenu();
void drawLoading();
void triggerBackend(bool reload);

// --- Init ---
void initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        std::cerr << "[C++ ERROR] SDL_Init failed: " << SDL_GetError() << std::endl;
        exit(1);
    }

    if (SDL_NumJoysticks() > 0) {
        SDL_JoystickOpen(0);
    }

    window   = SDL_CreateWindow("AMTube", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                 SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!window || !renderer) {
        std::cerr << "[C++ ERROR] SDL_CreateWindow/Renderer: " << SDL_GetError() << std::endl;
        exit(1);
    }

    std::string fontPath = std::string(RES_PATH) + "/NotoSans-Regular.ttf";
    if (!fontTitle.load(renderer, fontPath, 20.0f)) {
        std::cerr << "[C++ ERROR] Failed to load fontTitle from: " << fontPath << std::endl;
        exit(1);
    }
    if (!fontAuthor.load(renderer, fontPath, 16.0f)) {
        std::cerr << "[C++ ERROR] Failed to load fontAuthor from: " << fontPath << std::endl;
        exit(1);
    }
    std::cerr << "[C++ DEBUG] Init complete. Zero Dependency mode active." << std::endl;
}

// --- Load Data ---
SDL_Texture* loadTextureSTB(const std::string& path) {
    int w, h, channels;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4); // Force RGBA
    if (!data) {
        std::cerr << "[C++ ERROR] stbi_load failed: " << path << " - " << stbi_failure_reason() << std::endl;
        return nullptr;
    }
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(data, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
    SDL_Texture* tex = nullptr;
    if (surface) {
        tex = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
    }
    stbi_image_free(data);
    return tex;
}

void loadData() {
    for (auto& vid : videoList) {
        if (vid.texture) SDL_DestroyTexture(vid.texture);
    }
    videoList.clear();
    selectedIndex = 0;

    std::ifstream file("/tmp/yt_data/yt_data.txt");
    if (!file.is_open()) {
        std::cerr << "[C++ ERROR] Cannot open /tmp/yt_data/yt_data.txt" << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string item;
        YouTubeVideo vid;
        int col = 0;

        while (std::getline(ss, item, '|')) {
            if      (col == 0) vid.id         = item;
            else if (col == 1) vid.title       = item;
            else if (col == 2) vid.author      = item;
            else if (col == 3) vid.local_thumb = item;
            col++;
        }

        if (!vid.local_thumb.empty()) {
            vid.texture = loadTextureSTB(vid.local_thumb);
        }
        videoList.push_back(vid);
    }
    file.close();
    std::cerr << "[C++ DEBUG] Loaded " << videoList.size() << " videos." << std::endl;
}

// --- Draw ---
void drawList() {
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);

    // Header bar
    SDL_Rect headerRect = {0, 0, SCREEN_WIDTH, 40};
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderFillRect(renderer, &headerRect);
    fontTitle.renderText(renderer, 10, 28, "AMTube - " + currentCategory, {255, 255, 255, 255});
    fontAuthor.renderText(renderer, 350, 26, "Y:MENU X:RELOAD A:PLAY", {200, 200, 200, 255});

    int startY    = 50;
    int itemHeight = 90;
    int startIndex = selectedIndex - 1;
    if (startIndex < 0) startIndex = 0;

    for (int i = startIndex; i < startIndex + 4 && i < (int)videoList.size(); ++i) {
        int drawY = startY + (i - startIndex) * itemHeight;

        if (i == selectedIndex) {
            SDL_Rect hl  = {5, drawY,     SCREEN_WIDTH - 10, itemHeight - 5};
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderDrawRect(renderer, &hl);
            SDL_Rect hl2 = {4, drawY - 1, SCREEN_WIDTH - 8,  itemHeight - 3};
            SDL_RenderDrawRect(renderer, &hl2);
        }

        auto& vid = videoList[i];

        // Thumbnail
        if (vid.texture) {
            SDL_Rect tRect = {10, drawY + 5, 128, 72};
            SDL_RenderCopy(renderer, vid.texture, nullptr, &tRect);
        } else {
            SDL_Rect emptyBox = {10, drawY + 5, 128, 72};
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(renderer, &emptyBox);
        }

        // Text: title (wrapped) + author
        fontTitle.renderTextWrapped(renderer, 150, drawY + 20, vid.title,  {255, 255, 255, 255}, 470);
        fontAuthor.renderText(renderer, 150, drawY + 60, vid.author, {150, 150, 150, 255});
    }

    if (state == VIEW_MENU) {
        drawMenu();
    } else if (state == PLAYING_VIDEO) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        fontTitle.renderText(renderer, 170, 230, "Playing MPV... Press B to stop.", {255, 255, 255, 255});
    }

    SDL_RenderPresent(renderer);
}

void drawMenu() {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_Rect screenRect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &screenRect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    int boxW = 300, boxH = 260;
    int boxX = (SCREEN_WIDTH  - boxW) / 2;
    int boxY = (SCREEN_HEIGHT - boxH) / 2;

    SDL_Rect menuBox = {boxX, boxY, boxW, boxH};
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_RenderFillRect(renderer, &menuBox);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &menuBox);

    fontTitle.renderText(renderer, boxX + 40, boxY + 20, "--- CHUYEN MUC ---", {255, 200, 0, 255});

    for (int i = 0; i < (int)menuItems.size(); ++i) {
        int itemY = boxY + 52 + i * 30;

        if (i == menuIndex) {
            SDL_Rect hl = {boxX + 10, itemY - 2, boxW - 20, 28};
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
            SDL_RenderFillRect(renderer, &hl);
        }

        SDL_Color c = {255, 255, 255, 255};
        if (menuItems[i] == "Exit") c = {255, 50, 50, 255};

        fontTitle.renderText(renderer, boxX + 20, itemY + 16, menuItems[i], c);
    }
}

void drawLoading() {
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);
    fontTitle.renderText(renderer, 180, 230, "ZAPPING... Fetching data...", {255, 255, 255, 255});
    SDL_RenderPresent(renderer);
}

void triggerBackend(bool reload) {
    drawLoading();
    std::string mappedCategory = currentCategory;
    if (mappedCategory == "Kenh Dang Ky") mappedCategory = "Subscribed";

    std::string cmd = "./amtube_backend.sh --category \"" + mappedCategory + "\"";
    if (reload) cmd += " --reload";

    std::cerr << "[C++ DEBUG] Triggering backend: " << cmd << std::endl;
    int ret = system(cmd.c_str());
    std::cerr << "[C++ DEBUG] Backend returned: " << ret << std::endl;

    loadData();
}

void cleanup() {
    for (auto& vid : videoList) {
        if (vid.texture) SDL_DestroyTexture(vid.texture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(int argc, char* args[]) {
    initSDL();
    triggerBackend(false);

    SDL_Event e;
    while (isRunning) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                isRunning = false;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: isRunning = false; break;
                    case SDLK_UP:
                        if (state == VIEW_LIST && selectedIndex > 0) selectedIndex--;
                        else if (state == VIEW_MENU && menuIndex > 0) menuIndex--;
                        break;
                    case SDLK_DOWN:
                        if (state == VIEW_LIST && selectedIndex < (int)videoList.size() - 1) selectedIndex++;
                        else if (state == VIEW_MENU && menuIndex < (int)menuItems.size() - 1) menuIndex++;
                        break;
                }
            } else if (e.type == SDL_JOYBUTTONDOWN) {
                switch (e.jbutton.button) {
                    case 0: // B (Back)
                        if (state == VIEW_MENU) {
                            state = VIEW_LIST;
                        } else if (state == PLAYING_VIDEO) {
                            system("killall -9 mpv");
                            state = VIEW_LIST;
                            loadData();
                        }
                        break;
                    case 1: // A (Confirm / Play)
                        if (state == VIEW_MENU) {
                            if (menuItems[menuIndex] == "Exit") {
                                isRunning = false;
                            } else {
                                currentCategory = menuItems[menuIndex];
                                state = VIEW_LIST;
                                triggerBackend(false);
                            }
                        } else if (state == VIEW_LIST && !videoList.empty()) {
                            state = PLAYING_VIDEO;
                            // Free textures to release RAM before mpv
                            for (auto& vid : videoList) {
                                if (vid.texture) {
                                    SDL_DestroyTexture(vid.texture);
                                    vid.texture = nullptr;
                                }
                            }
                            std::string vidId = videoList[selectedIndex].id;
                            std::string cmd = "mpv --fs 'https://youtube.com/watch?v=" + vidId + "' &";
                            system(cmd.c_str());
                        }
                        break;
                    case 2: // Y (Menu)
                        if (state == VIEW_LIST) {
                            state = VIEW_MENU;
                        }
                        break;
                    case 3: // X (Reload / Zapping)
                        if (state == VIEW_LIST) {
                            triggerBackend(true);
                        }
                        break;
                }
            } else if (e.type == SDL_JOYHATMOTION) {
                if (e.jhat.value == SDL_HAT_UP) {
                    if (state == VIEW_LIST && selectedIndex > 0) selectedIndex--;
                    else if (state == VIEW_MENU && menuIndex > 0) menuIndex--;
                } else if (e.jhat.value == SDL_HAT_DOWN) {
                    if (state == VIEW_LIST && selectedIndex < (int)videoList.size() - 1) selectedIndex++;
                    else if (state == VIEW_MENU && menuIndex < (int)menuItems.size() - 1) menuIndex++;
                }
            }
        }
        if (state != PLAYING_VIDEO) {
            drawList();
        }
        SDL_Delay(16); // ~60 FPS
    }

    cleanup();
    return 0;
}
