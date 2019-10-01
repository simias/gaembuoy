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

static void gb_sdl_refresh_input(struct gb *gb) {
     SDL_Event e;

     while(SDL_PollEvent(&e)) {
          switch (e.type) {
          case SDL_QUIT:
               gb->quit = true;
               break;
          case SDL_KEYDOWN:
               switch (e.key.keysym.sym) {
               case SDLK_q:
               case SDLK_ESCAPE:
                    gb->quit = true;
                    break;
               }
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

     SDL_SetWindowTitle(ctx->window, "Gaembuoy");

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
