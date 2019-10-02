#include <SDL.h>
#include "gb.h"

struct gb_sdl_context {
     SDL_Window *window;
     SDL_Renderer *renderer;
     SDL_Texture *canvas;
     uint32_t pixels[GB_LCD_WIDTH * GB_LCD_HEIGHT];
};

static void gb_sdl_draw_line(struct gb *gb, unsigned ly,
                             enum gb_color line[GB_LCD_WIDTH]) {
     struct gb_sdl_context *ctx = gb->frontend.data;
     unsigned i;

     static const uint32_t col_map[4] = {
          [GB_COL_WHITE]     = 0xff75a32c,
          [GB_COL_LIGHTGREY] = 0xff387a21,
          [GB_COL_DARKGREY]  = 0xff255116,
          [GB_COL_BLACK]     = 0xff12280b,
     };

     for (i = 0; i < GB_LCD_WIDTH; i++) {
          ctx->pixels[ly * GB_LCD_WIDTH + i] = col_map[line[i]];
     }
}

static void gb_sdl_handle_key(struct gb *gb, SDL_Keycode key, bool pressed) {
     switch (key) {
     case SDLK_q:
     case SDLK_ESCAPE:
          gb->quit = pressed;
          break;
     case SDLK_RETURN:
          gb_input_set(gb, GB_INPUT_START, pressed);
          break;
     case SDLK_RSHIFT:
          gb_input_set(gb, GB_INPUT_SELECT, pressed);
          break;
     case SDLK_LCTRL:
          gb_input_set(gb, GB_INPUT_A, pressed);
          break;
     case SDLK_LSHIFT:
          gb_input_set(gb, GB_INPUT_B, pressed);
          break;
     case SDLK_UP:
          gb_input_set(gb, GB_INPUT_UP, pressed);
          break;
     case SDLK_DOWN:
          gb_input_set(gb, GB_INPUT_DOWN, pressed);
          break;
     case SDLK_LEFT:
          gb_input_set(gb, GB_INPUT_LEFT, pressed);
          break;
     case SDLK_RIGHT:
          gb_input_set(gb, GB_INPUT_RIGHT, pressed);
          break;
     }
}

static void gb_sdl_refresh_input(struct gb *gb) {
     SDL_Event e;

     while(SDL_PollEvent(&e)) {
          switch (e.type) {
          case SDL_QUIT:
               gb->quit = true;
               break;
          case SDL_KEYDOWN:
          case SDL_KEYUP:
               gb_sdl_handle_key(gb, e.key.keysym.sym,
                                 (e.key.state == SDL_PRESSED));
               break;
          }
     }
}

static void gb_sdl_flip(struct gb *gb) {
     struct gb_sdl_context *ctx = gb->frontend.data;

     /* Copy pixels to the canvas texture */
     SDL_UpdateTexture(ctx->canvas, NULL, ctx->pixels,
                       GB_LCD_WIDTH * sizeof(ctx->pixels[0]));

     /* Render the canvas */
     SDL_RenderCopy(ctx->renderer, ctx->canvas, NULL, NULL);
     SDL_RenderPresent(ctx->renderer);
}

static void gb_sdl_destroy(struct gb *gb) {
     struct gb_sdl_context *ctx = gb->frontend.data;

     SDL_DestroyTexture(ctx->canvas);
     SDL_DestroyRenderer(ctx->renderer);
     SDL_DestroyWindow(ctx->window);
     SDL_Quit();
     free(ctx);
     gb->frontend.data = NULL;
}

void gb_sdl_frontend_init(struct gb *gb) {
     struct gb_sdl_context *ctx;

     ctx = malloc(sizeof(*ctx));
     if (ctx == NULL) {
          perror("Malloc failed");
          die();
     }

     gb->frontend.data = ctx;

     if (SDL_Init(SDL_INIT_VIDEO) < 0) {
          fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
          die();
     }

     if (SDL_CreateWindowAndRenderer(GB_LCD_WIDTH, GB_LCD_HEIGHT,
                                     0, &ctx->window, &ctx->renderer) < 0) {
          fprintf(stderr, "SDL_CreateWindowAndRenderer failed: %s\n",
                  SDL_GetError());
          die();
     }

     ctx->canvas = SDL_CreateTexture(ctx->renderer,
                                     SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     GB_LCD_WIDTH, GB_LCD_HEIGHT);
     if (ctx->canvas == NULL) {
          fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
          die();
     }

     gb->frontend.draw_line = gb_sdl_draw_line;
     gb->frontend.flip = gb_sdl_flip;
     gb->frontend.refresh_input = gb_sdl_refresh_input;
     gb->frontend.destroy = gb_sdl_destroy;

     /* Clear the canvas */
     memset(ctx->pixels, 0, sizeof(ctx->pixels));
     gb_sdl_flip(gb);
}
