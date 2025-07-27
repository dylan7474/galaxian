/*
 * main.c - A classic Galaxian style game using SDL2
 *
 * This application is designed to be cross-compiled on a Linux system
 * to generate a standalone executable for Windows.
 * Features animated, diving enemies.
 *
 * Controls: Left/Right Arrow Keys to Move, Spacebar to Shoot.
 */

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

// --- Game Constants ---
#define SCREEN_WIDTH 600
#define SCREEN_HEIGHT 700
#define ALIEN_COLS 10
#define ALIEN_ROWS 5
#define SPRITE_SIZE 32
#define PLAYER_SPEED 5
#define BULLET_SPEED 10
#define MAX_PLAYER_BULLETS 2
#define SAMPLE_RATE 44100
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Structs ---
typedef enum { S_FORMATION, S_DIVING } AlienState;

typedef struct {
    float x, y;
    float formation_x, formation_y;
    int alive;
    int type; // 0=drone, 1=red, 2=flagship
    int anim_frame;
    AlienState state;
    float dive_angle;
    float dive_timer;
} Alien;

typedef struct {
    SDL_Rect rect;
} Player;

typedef struct {
    SDL_Rect rect;
    int active;
} Bullet;

// --- Global Variables ---
SDL_Window* g_window = NULL;
SDL_Renderer* g_renderer = NULL;
SDL_Texture* g_player_texture;
SDL_Texture* g_alien_textures[3][2]; // 3 types, 2 frames
Mix_Chunk* g_shoot_sound = NULL;
Mix_Chunk* g_explosion_sound = NULL;
Mix_Chunk* g_dive_sound = NULL;

Player g_player;
Alien g_aliens[ALIEN_ROWS][ALIEN_COLS];
Bullet g_player_bullets[MAX_PLAYER_BULLETS];
int g_formation_x = 0;
int g_formation_direction = 1;
int g_dive_timer = 0;

int g_score = 0;
int g_lives = 3;
int g_game_over = 0;

// --- Function Prototypes ---
int initialize();
void create_sprites();
void create_sounds();
void setup_level();
void handle_input(int* is_running);
void update_game();
void render_game();
void cleanup();
void fire_bullet();

// --- Main ---
int main(int argc, char* argv[]) {
    if (!initialize()) {
        cleanup();
        return 1;
    }
    setup_level();
    int is_running = 1;
    while (is_running && !g_game_over) {
        handle_input(&is_running);
        update_game();
        render_game();
        SDL_Delay(16);
    }
    cleanup();
    return 0;
}

// --- Implementations ---
int initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return 0;
    if (Mix_OpenAudio(SAMPLE_RATE, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return 0;
    g_window = SDL_CreateWindow("SDL Galaxian", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!g_window) return 0;
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) return 0;
    srand(time(0));
    create_sprites();
    create_sounds();
    return 1;
}

void create_sounds() {
    { // Shoot
        static Sint16 d[SAMPLE_RATE/20]; int l=sizeof(d);
        for(int i=0;i<l/2;++i){double t=(double)i/SAMPLE_RATE; d[i]=(Sint16)(3000*sin(2.0*M_PI*1200.0*t)*(1.0-(t*20)));}
        g_shoot_sound=Mix_QuickLoad_RAW((Uint8*)d,l);
    }
    { // Explosion
        static Sint16 d[SAMPLE_RATE/8]; int l=sizeof(d);
        for(int i=0;i<l/2;++i){double t=(double)i/SAMPLE_RATE;double f=440-(t*800);d[i]=(Sint16)(6000*sin(2.0*M_PI*f*t)*(1.0-(t*8)) + 2000*((rand()%256)/255.0-0.5));}
        g_explosion_sound=Mix_QuickLoad_RAW((Uint8*)d,l);
    }
    { // Dive
        static Sint16 d[SAMPLE_RATE/2]; int l=sizeof(d);
        for(int i=0;i<l/2;++i){double t=(double)i/SAMPLE_RATE;double f=1500-(t*2500);d[i]=(Sint16)(4000*sin(2.0*M_PI*f*t)*(1.0-(t*2)));}
        g_dive_sound=Mix_QuickLoad_RAW((Uint8*)d,l);
    }
}

void create_sprites() {
    // Player
    g_player_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, SPRITE_SIZE, SPRITE_SIZE);
    SDL_SetTextureBlendMode(g_player_texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(g_renderer, g_player_texture);
    SDL_SetRenderDrawColor(g_renderer, 0,0,0,0); SDL_RenderClear(g_renderer);
    SDL_SetRenderDrawColor(g_renderer, 255,255,255,255);
    SDL_RenderFillRect(g_renderer, &(SDL_Rect){12,0,8,4});
    SDL_RenderFillRect(g_renderer, &(SDL_Rect){8,4,16,16});
    SDL_SetRenderDrawColor(g_renderer, 255,0,0,255);
    SDL_RenderFillRect(g_renderer, &(SDL_Rect){0,12,8,4});
    SDL_RenderFillRect(g_renderer, &(SDL_Rect){24,12,8,4});
    
    // Aliens
    for(int type=0; type<3; ++type) {
        for(int frame=0; frame<2; ++frame) {
            g_alien_textures[type][frame] = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, SPRITE_SIZE, SPRITE_SIZE);
            SDL_SetTextureBlendMode(g_alien_textures[type][frame], SDL_BLENDMODE_BLEND);
            SDL_SetRenderTarget(g_renderer, g_alien_textures[type][frame]);
            SDL_SetRenderDrawColor(g_renderer, 0,0,0,0); SDL_RenderClear(g_renderer);
            if(type==0){ // Drone
                SDL_SetRenderDrawColor(g_renderer, 0,255,255,255);
                SDL_RenderFillRect(g_renderer, &(SDL_Rect){12,4,8,4});
                SDL_RenderFillRect(g_renderer, &(SDL_Rect){8,8,16,12});
                SDL_RenderFillRect(g_renderer, &(SDL_Rect){4,12,8,4});
                SDL_RenderFillRect(g_renderer, &(SDL_Rect){20,12,8,4});
                SDL_SetRenderDrawColor(g_renderer, 255,0,0,255);
                if(frame==0){SDL_RenderFillRect(g_renderer, &(SDL_Rect){12,20,8,4});} else {SDL_RenderFillRect(g_renderer, &(SDL_Rect){8,20,16,4});}
            } else if (type==1){ // Red
                SDL_SetRenderDrawColor(g_renderer, 255,0,0,255);
                SDL_RenderFillRect(g_renderer, &(SDL_Rect){12,4,8,4});
                SDL_RenderFillRect(g_renderer, &(SDL_Rect){8,8,16,12});
                SDL_RenderFillRect(g_renderer, &(SDL_Rect){4,12,8,4});
                SDL_RenderFillRect(g_renderer, &(SDL_Rect){20,12,8,4});
                SDL_SetRenderDrawColor(g_renderer, 255,255,0,255);
                if(frame==0){SDL_RenderFillRect(g_renderer, &(SDL_Rect){12,20,8,4});} else {SDL_RenderFillRect(g_renderer, &(SDL_Rect){8,20,16,4});}
            } else { // Flagship
                 SDL_SetRenderDrawColor(g_renderer, 255,255,0,255);
                 SDL_RenderFillRect(g_renderer, &(SDL_Rect){12,4,8,4});
                 SDL_RenderFillRect(g_renderer, &(SDL_Rect){8,8,16,8});
                 SDL_RenderFillRect(g_renderer, &(SDL_Rect){4,12,8,8});
                 SDL_RenderFillRect(g_renderer, &(SDL_Rect){20,12,8,8});
                 SDL_SetRenderDrawColor(g_renderer, 255,0,0,255);
                 if(frame==0){SDL_RenderFillRect(g_renderer, &(SDL_Rect){8,20,4,4}); SDL_RenderFillRect(g_renderer, &(SDL_Rect){20,20,4,4});}
                 else{SDL_RenderFillRect(g_renderer, &(SDL_Rect){12,20,8,4});}
            }
        }
    }
    SDL_SetRenderTarget(g_renderer, NULL);
}

void setup_level() {
    g_player.rect = (SDL_Rect){SCREEN_WIDTH/2 - SPRITE_SIZE/2, SCREEN_HEIGHT - 60, SPRITE_SIZE, SPRITE_SIZE};
    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            g_aliens[r][c].formation_x = c * 45 + 75;
            g_aliens[r][c].formation_y = r * 40 + 60;
            g_aliens[r][c].x = g_aliens[r][c].formation_x;
            g_aliens[r][c].y = g_aliens[r][c].formation_y;
            g_aliens[r][c].alive = 1;
            g_aliens[r][c].anim_frame = 0;
            g_aliens[r][c].state = S_FORMATION;
            if(r < 2) g_aliens[r][c].type = 2; // Flagships
            else if (r < 4) g_aliens[r][c].type = 1; // Reds
            else g_aliens[r][c].type = 0; // Drones
        }
    }
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) g_player_bullets[i].active = 0;
}

void fire_bullet() {
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (!g_player_bullets[i].active) {
            g_player_bullets[i].rect = (SDL_Rect){g_player.rect.x + SPRITE_SIZE/2 - 2, g_player.rect.y, 4, 12};
            g_player_bullets[i].active = 1;
            if(g_shoot_sound) Mix_PlayChannel(-1, g_shoot_sound, 0);
            return;
        }
    }
}

void handle_input(int* is_running) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) *is_running = 0;
    }
    const Uint8* state = SDL_GetKeyboardState(NULL);
    if (state[SDL_SCANCODE_LEFT] && g_player.rect.x > 0) g_player.rect.x -= PLAYER_SPEED;
    if (state[SDL_SCANCODE_RIGHT] && g_player.rect.x < SCREEN_WIDTH - g_player.rect.w) g_player.rect.x += PLAYER_SPEED;
    if (state[SDL_SCANCODE_SPACE]) {
        static Uint32 last_shot = 0;
        if (SDL_GetTicks() - last_shot > 300) {
            fire_bullet();
            last_shot = SDL_GetTicks();
        }
    }
}

void update_game() {
    // --- Update Player Bullets & Collisions ---
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (g_player_bullets[i].active) {
            g_player_bullets[i].rect.y -= BULLET_SPEED;
            if (g_player_bullets[i].rect.y < 0) g_player_bullets[i].active = 0;

            for(int r=0; r<ALIEN_ROWS; ++r) for(int c=0; c<ALIEN_COLS; ++c) {
                if (g_aliens[r][c].alive) {
                    SDL_Rect alien_rect = {(int)g_aliens[r][c].x, (int)g_aliens[r][c].y, SPRITE_SIZE, SPRITE_SIZE};
                    if (SDL_HasIntersection(&g_player_bullets[i].rect, &alien_rect)) {
                        g_aliens[r][c].alive = 0;
                        g_player_bullets[i].active = 0;
                        g_score += (g_aliens[r][c].type + 1) * 50;
                        if(g_explosion_sound) Mix_PlayChannel(-1, g_explosion_sound, 0);
                    }
                }
            }
        }
    }

    // --- Update Alien Formation ---
    static int anim_timer = 0;
    if(++anim_timer > 30) {
        anim_timer = 0;
        for(int r=0; r<ALIEN_ROWS; ++r) for(int c=0; c<ALIEN_COLS; ++c) {
            g_aliens[r][c].anim_frame = 1 - g_aliens[r][c].anim_frame;
        }
    }

    g_formation_x += g_formation_direction;
    if(abs(g_formation_x) > 30) g_formation_direction *= -1;
    
    // --- Update Individual Aliens ---
    if (++g_dive_timer > 100) { // Time to send a new diver
        g_dive_timer = 0;
        int r = rand() % ALIEN_ROWS;
        int c = rand() % ALIEN_COLS;
        if(g_aliens[r][c].alive && g_aliens[r][c].state == S_FORMATION) {
            g_aliens[r][c].state = S_DIVING;
            g_aliens[r][c].dive_timer = 0;
            if(g_dive_sound) Mix_PlayChannel(-1, g_dive_sound, 0);
        }
    }
    
    for(int r=0; r<ALIEN_ROWS; ++r) for(int c=0; c<ALIEN_COLS; ++c) {
        if(g_aliens[r][c].alive) {
            if (g_aliens[r][c].state == S_FORMATION) {
                g_aliens[r][c].x = g_aliens[r][c].formation_x + g_formation_x;
            } else { // Is S_DIVING
                g_aliens[r][c].dive_timer += 0.03f;
                g_aliens[r][c].x = g_aliens[r][c].formation_x + sinf(g_aliens[r][c].dive_timer) * 100;
                g_aliens[r][c].y += 3;
                if(g_aliens[r][c].y > SCREEN_HEIGHT) {
                    g_aliens[r][c].y = g_aliens[r][c].formation_y;
                    g_aliens[r][c].state = S_FORMATION;
                }
            }
            SDL_Rect alien_rect = {(int)g_aliens[r][c].x, (int)g_aliens[r][c].y, SPRITE_SIZE, SPRITE_SIZE};
            if(SDL_HasIntersection(&g_player.rect, &alien_rect)){
                 g_lives--;
                 if(g_explosion_sound) Mix_PlayChannel(-1, g_explosion_sound, 0);
                 if(g_lives <= 0) g_game_over = 1;
                 else {
                     g_player.rect.x = SCREEN_WIDTH/2 - SPRITE_SIZE/2;
                     g_aliens[r][c].alive = 0;
                     SDL_Delay(1000);
                 }
            }
        }
    }
}

void render_game() {
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    
    // Draw player
    SDL_RenderCopy(g_renderer, g_player_texture, NULL, &g_player.rect);
    
    // Draw aliens
    for (int r=0; r<ALIEN_ROWS; ++r) for (int c=0; c<ALIEN_COLS; ++c) {
        if(g_aliens[r][c].alive) {
            SDL_Rect dest = {(int)g_aliens[r][c].x, (int)g_aliens[r][c].y, SPRITE_SIZE, SPRITE_SIZE};
            SDL_RenderCopy(g_renderer, g_alien_textures[g_aliens[r][c].type][g_aliens[r][c].anim_frame], NULL, &dest);
        }
    }
    
    // Draw player bullets
    SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);
    for(int i=0; i<MAX_PLAYER_BULLETS; ++i) {
        if(g_player_bullets[i].active) {
            SDL_RenderFillRect(g_renderer, &g_player_bullets[i].rect);
        }
    }
    
    SDL_RenderPresent(g_renderer);
}

void cleanup() {
    if(g_shoot_sound) Mix_FreeChunk(g_shoot_sound);
    if(g_explosion_sound) Mix_FreeChunk(g_explosion_sound);
    if(g_dive_sound) Mix_FreeChunk(g_dive_sound);
    Mix_CloseAudio();
    if (g_player_texture) SDL_DestroyTexture(g_player_texture);
    for(int i=0; i<3; ++i) {
        if(g_alien_textures[i][0]) SDL_DestroyTexture(g_alien_textures[i][0]);
        if(g_alien_textures[i][1]) SDL_DestroyTexture(g_alien_textures[i][1]);
    }
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    SDL_Quit();
}
