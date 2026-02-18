#include <SDL.h>
#include <emscripten.h>

SDL_Window* window;
SDL_Renderer* renderer;

void loop() {
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(
        "WASM Window",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN
    );

    renderer = SDL_CreateRenderer(window, -1, 0);

    emscripten_set_main_loop(loop, 0, 1);
}
