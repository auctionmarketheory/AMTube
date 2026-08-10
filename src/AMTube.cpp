#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>

#ifndef RES_PATH
#define RES_PATH "./res"
#endif

// --- Constants ---
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

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
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
TTF_Font* fontTitle = nullptr;
TTF_Font* fontAuthor = nullptr;

std::vector<YouTubeVideo> videoList;
int selectedIndex = 0;
bool isRunning = true;

std::string currentCategory = "Subscribed"; // Khởi động bằng Zapping Kênh Đăng Ký
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

// --- Function Prototypes ---
void initSDL();
void cleanup();
void loadData();
void drawText(const std::string& text, int x, int y, TTF_Font* font, SDL_Color color);
void drawList();
void drawMenu();
void drawLoading();

// --- Functions ---
void initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        exit(1);
    }
    
    if (SDL_NumJoysticks() > 0) {
        SDL_JoystickOpen(0);
    }

    if (TTF_Init() == -1) {
        std::cerr << "TTF_Init: " << TTF_GetError() << std::endl;
        exit(1);
    }

    if (!(IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG) & (IMG_INIT_JPG | IMG_INIT_PNG))) {
        std::cerr << "IMG_Init Error: " << IMG_GetError() << std::endl;
        exit(1);
    }

    window = SDL_CreateWindow("AMTube", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    std::string fontPath = std::string(RES_PATH) + "/NotoSans-Regular.ttf";
    fontTitle = TTF_OpenFont(fontPath.c_str(), 20);
    fontAuthor = TTF_OpenFont(fontPath.c_str(), 16);
}

void loadData() {
    for (auto& vid : videoList) {
        if (vid.texture) {
            SDL_DestroyTexture(vid.texture);
        }
    }
    videoList.clear();
    selectedIndex = 0;

    std::ifstream file("/tmp/yt_data/yt_data.txt");
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string item;
        YouTubeVideo vid;
        int col = 0;
        
        while (std::getline(ss, item, '|')) {
            if (col == 0) vid.id = item;
            else if (col == 1) vid.title = item;
            else if (col == 2) vid.author = item;
            else if (col == 3) vid.local_thumb = item;
            col++;
        }
        
        if (!vid.local_thumb.empty()) {
            SDL_Surface* surface = IMG_Load(vid.local_thumb.c_str());
            if (surface) {
                vid.texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FreeSurface(surface);
            }
        }
        
        videoList.push_back(vid);
    }
    file.close();
}

void drawText(const std::string& text, int x, int y, TTF_Font* font, SDL_Color color) {
    if (!font || text.empty()) return;
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_Rect rect = { x, y, surface->w, surface->h };
        SDL_RenderCopy(renderer, texture, nullptr, &rect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
    }
}

void drawList() {
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);

    SDL_Rect headerRect = {0, 0, SCREEN_WIDTH, 40};
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderFillRect(renderer, &headerRect);
    drawText("AMTube - " + currentCategory, 10, 10, fontTitle, {255, 255, 255, 255});
    drawText("Y: MENU | X: RELOAD | A: PLAY", 350, 12, fontAuthor, {200, 200, 200, 255});

    int startY = 50;
    int itemHeight = 90; 
    
    int startIndex = selectedIndex - 1;
    if (startIndex < 0) startIndex = 0;
    
    for (int i = startIndex; i < startIndex + 4 && i < (int)videoList.size(); ++i) {
        int drawY = startY + (i - startIndex) * itemHeight;
        
        if (i == selectedIndex) {
            SDL_Rect hl = {5, drawY, SCREEN_WIDTH - 10, itemHeight - 5};
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderDrawRect(renderer, &hl);
            SDL_Rect hl2 = {4, drawY - 1, SCREEN_WIDTH - 8, itemHeight - 3};
            SDL_RenderDrawRect(renderer, &hl2);
        }

        auto& vid = videoList[i];
        
        if (vid.texture) {
            SDL_Rect tRect = {10, drawY + 5, 128, 72};
            SDL_RenderCopy(renderer, vid.texture, nullptr, &tRect);
        } else {
            SDL_Rect emptyBox = {10, drawY + 5, 128, 72};
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_RenderFillRect(renderer, &emptyBox);
        }
        
        drawText(vid.title, 150, drawY + 10, fontTitle, {255, 255, 255, 255});
        drawText(vid.author, 150, drawY + 40, fontAuthor, {150, 150, 150, 255});
    }

    if (state == VIEW_MENU) {
        drawMenu();
    } else if (state == PLAYING_VIDEO) {
        // Màn hình đen khi Video Play ngầm
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        drawText("Playing MPV... Press B to stop.", 180, 220, fontTitle, {255, 255, 255, 255});
    }

    SDL_RenderPresent(renderer);
}

void drawMenu() {
    // Semi-transparent overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_Rect screenRect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &screenRect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // Menu Box
    int boxW = 300;
    int boxH = 260;
    int boxX = (SCREEN_WIDTH - boxW) / 2;
    int boxY = (SCREEN_HEIGHT - boxH) / 2;

    SDL_Rect menuBox = {boxX, boxY, boxW, boxH};
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_RenderFillRect(renderer, &menuBox);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &menuBox);

    drawText("--- CHUYEN MUC ---", boxX + 50, boxY + 10, fontTitle, {255, 200, 0, 255});

    for (int i = 0; i < (int)menuItems.size(); ++i) {
        int itemY = boxY + 50 + i * 30;
        
        if (i == menuIndex) {
            SDL_Rect hl = {boxX + 10, itemY - 2, boxW - 20, 28};
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
            SDL_RenderFillRect(renderer, &hl);
        }

        SDL_Color c = {255, 255, 255, 255};
        if (menuItems[i] == "Exit") c = {255, 50, 50, 255};
        
        drawText(menuItems[i], boxX + 20, itemY, fontTitle, c);
    }
}

void drawLoading() {
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);
    drawText("ZAPPING... Fetching data...", 180, 220, fontTitle, {255, 255, 255, 255});
    SDL_RenderPresent(renderer);
}

void triggerBackend(bool reload) {
    drawLoading();
    std::string mappedCategory = currentCategory;
    if (mappedCategory == "Kenh Dang Ky") mappedCategory = "Subscribed";

    std::string cmd = "/home/amt/Màn\\ hình\\ nền/R36S/App_AMTube_Source/amtube_backend.sh --category \"" + mappedCategory + "\"";
    if (reload) cmd += " --reload";
    system(cmd.c_str());
    loadData();
}

void cleanup() {
    for (auto& vid : videoList) {
        if (vid.texture) {
            SDL_DestroyTexture(vid.texture);
        }
    }
    TTF_CloseFont(fontTitle);
    TTF_CloseFont(fontAuthor);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
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
                    case SDLK_ESCAPE:
                        isRunning = false; 
                        break;
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
                            // Hijack V2: Kill MPV
                            system("killall -9 mpv");
                            state = VIEW_LIST;
                            // Phục hồi texture
                            loadData();
                        }
                        break;
                    case 1: // A (Confirm)
                        if (state == VIEW_MENU) {
                            if (menuItems[menuIndex] == "Exit") {
                                isRunning = false;
                            } else {
                                currentCategory = menuItems[menuIndex];
                                state = VIEW_LIST;
                                triggerBackend(false);
                            }
                        } else if (state == VIEW_LIST && !videoList.empty()) {
                            // Phát video bằng MPV (The Hijack V2)
                            state = PLAYING_VIDEO;
                            drawList(); // Update UI to black screen with text
                            // Xóa texture để giải phóng RAM
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
                    case 3: // X (Reload/Zapping)
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
