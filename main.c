#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "types.h"

#define PI 3.14159265359

#define TARGET_FPS 30
#define MS_PER_FRAME (1000.0 / TARGET_FPS)

#define MAP_WIDTH 14
#define MAP_HEIGHT 8
#define MAP_SCALE 50
#define PLAYER_SIZE (1 + MAP_SCALE / 5)

#define VIEWPORT_WIDTH (MAP_WIDTH * MAP_SCALE)
#define VIEWPORT_HEIGHT (MAP_HEIGHT * MAP_SCALE)

#define SCREEN_WIDTH (2 * VIEWPORT_WIDTH)
#define SCREEN_HEIGHT VIEWPORT_HEIGHT

#define RAYCAST_X_OFFSET (MAP_WIDTH * MAP_SCALE)

#define RAYCAST_MAX_DIST (10.0 * MAP_SCALE)
#define RAYCAST_STEP (0.01 * MAP_SCALE)

// clang-format off
const u8 map[MAP_WIDTH * MAP_HEIGHT] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 1,
    1, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 1,
    1, 0, 2, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 3, 3, 3, 5, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};
// clang-format on

const u32 color_map[6] = {
    0x00000000,  // black
    0xffffffff,  // white
    0xff0000ff,  // red
    0x00ff00ff,  // green
    0x0000ffff,  // blue
    0xffff00ff,  // yellow
};

const u32 player_color = 0xff00ffff;  // purple

// ================== GAME STATE ==================
struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    u32 pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
    bool running;
} state;

struct {
    vec2 pos, rot;
    f32 angle, fov;
} player;

f32 start_render_x = 0.0;
f32 end_render_x = VIEWPORT_WIDTH;
SDL_TimerID last_time_pressed = 0;
// =================================================

// scale up map coordinates
f32 to_world(f32 world) {
    return world * MAP_SCALE;
}

// scale down world coordinates
f32 to_map(f32 map) {
    return map / MAP_SCALE;
}

f32 to_rad(f32 deg) {
    return deg * PI / 180.0;
}

void set_pixel(u32 x, u32 y, u32 color) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
        return;
    }
    state.pixels[y * SCREEN_WIDTH + x] = color;
}

void draw_line(ivec2 p0, ivec2 p1, u32 color) {
    bool steep = false;
    if (abs(p0.x - p1.x) < abs(p0.y - p1.y)) {
        i32 temp = p0.x;
        p0.x = p0.y;
        p0.y = temp;

        temp = p1.x;
        p1.x = p1.y;
        p1.y = temp;
        steep = true;
    }

    if (p0.x > p1.x) {
        i32 temp = p0.x;
        p0.x = p1.x;
        p1.x = temp;

        temp = p0.y;
        p0.y = p1.y;
        p1.y = temp;
    }

    i32 dx = p1.x - p0.x;
    i32 dy = p1.y - p0.y;
    i32 derror2 = abs(dy) * 2;
    i32 error2 = 0;
    i32 y = p0.y;

    i32 y_error_step = (p1.y > p0.y ? 1 : -1);

    for (i32 x = p0.x; x <= p1.x; x++) {
        if (steep) {
            set_pixel(y, x, color);
        } else {
            set_pixel(x, y, color);
        }

        error2 += derror2;
        if (error2 > dx) {
            y += y_error_step;
            error2 -= dx * 2;
        }
    }
}

void top_down_display() {
    // draw map
    for (u32 y = 0; y < MAP_HEIGHT; y++) {
        for (u32 x = 0; x < MAP_WIDTH; x++) {
            u8 tile = map[y * MAP_WIDTH + x];

            for (u32 j = 0; j < MAP_SCALE; j++) {
                for (u32 i = 0; i < MAP_SCALE; i++) {
                    set_pixel(
                        to_world(x) + i, to_world(y) + j, color_map[tile]);
                }
            }
        }
    }

    // draw player
    for (i32 j = -PLAYER_SIZE / 2; j < PLAYER_SIZE / 2; j++) {
        for (i32 i = -PLAYER_SIZE / 2; i < PLAYER_SIZE / 2; i++) {
            set_pixel(to_world(player.pos.x) + i,
                      to_world(player.pos.y) + j,
                      player_color);
        }
    }

    // draw player direction
    // ivec2 p0 = {player.pos.x * MAP_SCALE, player.pos.y * MAP_SCALE};
    // ivec2 p1 = {p0.x + to_world(player.rot.x) * 3,
    //            p0.y + to_world(player.rot.y) * 3};
    // draw_line(p0, p1, player_color);
}

void raycast() {
    ivec2 map_pos = {to_world(player.pos.x), to_world(player.pos.y)};

    // draw rays
    f32 fov = player.fov;
    f32 step = fov / VIEWPORT_WIDTH;
    f32 initial_angle = player.angle - fov / 2.0;

    for (f32 i = start_render_x; i < end_render_x; i += 1.0) {
        f32 ray_angle = initial_angle + (i * step);
        vec2 ray_dir = {cos(ray_angle), sin(ray_angle)};

        f32 dist = 0.0;
        u8 tile = 0;
        while (dist < RAYCAST_MAX_DIST) {
            ivec2 ray_pos = {map_pos.x + ray_dir.x * dist,
                             map_pos.y + ray_dir.y * dist};

            // check bounds
            if (ray_pos.x < 0 || ray_pos.x >= to_world(MAP_WIDTH) ||
                ray_pos.y < 0 || ray_pos.y >= to_world(MAP_HEIGHT)) {
                break;
            }

            tile = map[(u32)to_map(ray_pos.y) * MAP_WIDTH +
                       (u32)to_map(ray_pos.x)];

            if (tile != 0)
                break;

            dist += RAYCAST_STEP;
        }

        if (tile != 0) {
            // draw vertical wall in first person view

            // normalize to remove fisheye effect
            f32 norm_dist = dist * cos(player.angle - ray_angle);
            i32 wall_height = (i32)(VIEWPORT_HEIGHT * MAP_SCALE / norm_dist);

            ivec2 p0 = {RAYCAST_X_OFFSET + i,
                        VIEWPORT_HEIGHT / 2 - wall_height / 2};
            ivec2 p1 = {RAYCAST_X_OFFSET + i,
                        VIEWPORT_HEIGHT / 2 + wall_height / 2};

            draw_line(p0, p1, color_map[tile]);
        }

        // draw ray in top down view
        ivec2 p0 = {to_world(player.pos.x), to_world(player.pos.y)};
        ivec2 p1 = {p0.x + ray_dir.x * dist, p0.y + ray_dir.y * dist};
        draw_line(p0, p1, 0xd4d4d4ff);
    }
}

int main(int argc, char *argv[]) {
    ASSERT(
        SDL_Init(SDL_INIT_VIDEO) == 0, "SDL_Init failed: %s\n", SDL_GetError());

    state.window =
        SDL_CreateWindow("Raycaster visualizer - press F to toggle single ray mode",
                         SDL_WINDOWPOS_CENTERED_DISPLAY(0),
                         SDL_WINDOWPOS_CENTERED_DISPLAY(0),
                         SCREEN_WIDTH,
                         SCREEN_HEIGHT,
                         SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
    ASSERT(state.window, "SDL_CreateWindow failed: %s\n", SDL_GetError());

    state.renderer =
        SDL_CreateRenderer(state.window, -1, SDL_RENDERER_ACCELERATED);
    ASSERT(state.renderer, "SDL_CreateRenderer failed: %s\n", SDL_GetError());

    state.texture = SDL_CreateTexture(state.renderer,
                                      SDL_PIXELFORMAT_RGBA8888,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      SCREEN_WIDTH,
                                      SCREEN_HEIGHT);
    ASSERT(state.texture, "SDL_CreateTexture failed: %s\n", SDL_GetError());

    player.pos = (vec2){2.0, 2.0};
    player.angle = 0.0;
    player.fov = to_rad(90.0);
    player.rot = (vec2){cos(player.angle) * 0.1, sin(player.angle) * 0.1};
    state.running = true;

    SDL_TimerID old_time = SDL_GetTicks();
    SDL_TimerID time = old_time;

    while (state.running) {
        old_time = time;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    state.running = false;
                    break;
            }
        }

        // ==================
        // MOVEMENT
        // ==================

        ASSERT(player.pos.x >= 0.0 && player.pos.x < MAP_WIDTH,
               "Player x coord out of bounds!\n");
        ASSERT(player.pos.y >= 0.0 && player.pos.y < MAP_HEIGHT,
               "Player y coord out of bounds!\n");

        const u8 *keys = SDL_GetKeyboardState(NULL);

        if (keys[SDL_SCANCODE_ESCAPE]) {
            state.running = false;
        }

        // handle movement
        if (keys[SDL_SCANCODE_W]) {
            player.pos.x += player.rot.x;
            player.pos.y += player.rot.y;

            // check collision
            u8 tile = map[(u32)player.pos.y * MAP_WIDTH + (u32)player.pos.x];
            if (tile != 0) {
                player.pos.x -= player.rot.x;
                player.pos.y -= player.rot.y;
            }
        }
        if (keys[SDL_SCANCODE_S]) {
            player.pos.x -= player.rot.x;
            player.pos.y -= player.rot.y;

            // check collision
            u8 tile = map[(u32)player.pos.y * MAP_WIDTH + (u32)player.pos.x];
            if (tile != 0) {
                player.pos.x += player.rot.x;
                player.pos.y += player.rot.y;
            }
        }

        // handle rotation
        if (keys[SDL_SCANCODE_A]) {
            player.angle -= 0.1;
            if (player.angle < 0.0) {
                player.angle += 2 * PI;
            }
            player.rot.x = cos(player.angle) * 0.1;
            player.rot.y = sin(player.angle) * 0.1;
        }
        if (keys[SDL_SCANCODE_D]) {
            player.angle += 0.1;
            if (player.angle > 2 * PI) {
                player.angle -= 2 * PI;
            }
            player.rot.x = cos(player.angle) * 0.1;
            player.rot.y = sin(player.angle) * 0.1;
        }

        // bound player to map
        if (player.pos.x < 1.0) {
            player.pos.x = 1.0;
        } else if (player.pos.x > MAP_WIDTH - 1.0) {
            player.pos.x = MAP_WIDTH - 1.0;
        }
        if (player.pos.y < 1.0) {
            player.pos.y = 1.0;
        } else if (player.pos.y > MAP_HEIGHT - 1.0) {
            player.pos.y = MAP_HEIGHT - 1.0;
        }


        if (keys[SDL_SCANCODE_F]) {
            // check if enough time has passed since last press
            if (SDL_GetTicks() - last_time_pressed > 300) {
                last_time_pressed = SDL_GetTicks();
                if (start_render_x == 0.0) {
                    start_render_x = VIEWPORT_WIDTH / 2.0 - 1*MAP_SCALE/20.0;
                    end_render_x = VIEWPORT_WIDTH / 2.0 + 1*MAP_SCALE/20.0;
                } else {
                    start_render_x = 0.0;
                    end_render_x = VIEWPORT_WIDTH;
                }
            }
        }

        // ==================
        // RENDERING
        // ==================

        // flush
        memset(state.pixels, 0, sizeof(state.pixels));

        // render
        top_down_display();
        raycast();

        // render to screen
        SDL_UpdateTexture(
            state.texture, NULL, state.pixels, SCREEN_WIDTH * sizeof(u32));
        SDL_RenderCopyEx(state.renderer,
                         state.texture,
                         NULL,
                         NULL,
                         0.0,
                         NULL,
                         SDL_FLIP_NONE);

        SDL_RenderPresent(state.renderer);

        // cap framerate
        time = SDL_GetTicks();
        f32 frame_time = (time - old_time) / 1000.0;
        if (frame_time < MS_PER_FRAME) {
            SDL_Delay(MS_PER_FRAME - frame_time);
        }
        printf("FPS: %f\n", 1.0 / frame_time);
    }

    SDL_DestroyWindow(state.window);
    SDL_DestroyRenderer(state.renderer);
    SDL_DestroyTexture(state.texture);

    printf("Exited normally!\n");
    return 0;
}
