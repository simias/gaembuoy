#include <SDL.h>
#include <assert.h>
#include "gb.h"

#define UPSCALE_FACTOR 4

struct gb_sdl_context {
     SDL_Window *window;
     SDL_Renderer *renderer;
     SDL_Texture *canvas;
     SDL_GameController *controller;
     SDL_AudioSpec audio_spec;
     SDL_AudioDeviceID audio_device;
     uint32_t pixels[GB_LCD_WIDTH * GB_LCD_HEIGHT * UPSCALE_FACTOR * UPSCALE_FACTOR];
     /* Index of the next audio buffer we want to play */
     unsigned audio_buf_index;
};

static void gb_sdl_draw_line_dmg(struct gb *gb, unsigned ly,
                                 union gb_gpu_color line[GB_LCD_WIDTH]) {
     struct gb_sdl_context *ctx = gb->frontend.data;
     unsigned i;
     unsigned x;
     unsigned y;

     static const uint32_t col_map[4] = {
          [GB_COL_WHITE]     = 0xff75a32c,
          [GB_COL_LIGHTGREY] = 0xff387a21,
          [GB_COL_DARKGREY]  = 0xff255116,
          [GB_COL_BLACK]     = 0xff12280b,
     };

     for (i = 0; i < GB_LCD_WIDTH; i++) {
          for (y = 0; y < UPSCALE_FACTOR; y++) {
               for (x = 0; x < UPSCALE_FACTOR; x++) {
                    ctx->pixels[(ly + y) * GB_LCD_WIDTH * UPSCALE_FACTOR + i + x] = col_map[line[i].dmg_color];
               }
          }
     }
}

static uint32_t gb_sdl_5_to_8bits(uint32_t v) {
     return (v << 3) | (v >> 2);
}

static uint32_t gb_sdl_gbc_to_xrgb8888(uint16_t c) {
     uint32_t r = c & 0x1f;
     uint32_t g = (c >> 5) & 0x1f;
     uint32_t b = (c >> 10) & 0x1f;
     uint32_t p;

     /* Extend from 5 to 8 bits */
     r = gb_sdl_5_to_8bits(r);
     g = gb_sdl_5_to_8bits(g);
     b = gb_sdl_5_to_8bits(b);

     p = 0xff000000;
     p |= r << 16;
     p |= g << 8;
     p |= b;

     return p;
}

static void gb_sdl_draw_line_gbc(struct gb *gb, unsigned ly,
                                 union gb_gpu_color line[GB_LCD_WIDTH]) {
     struct gb_sdl_context *ctx = gb->frontend.data;
     unsigned i;
     unsigned x;
     unsigned y;

     for (i = 0; i < GB_LCD_WIDTH; i++) {
          uint16_t c = line[i].gbc_color;

          for (y = 0; y < UPSCALE_FACTOR; y++) {
               for (x = 0; x < UPSCALE_FACTOR; x++) {
                    ctx->pixels[(ly + y) * GB_LCD_WIDTH * UPSCALE_FACTOR + i + x] = gb_sdl_gbc_to_xrgb8888(c);
               }
          }
     }
}

static void gb_sdl_handle_key(struct gb *gb, SDL_Keycode key, bool pressed) {
     switch (key) {
     case SDLK_q:
     case SDLK_ESCAPE:
          if (pressed) {
               gb->quit = true;
          }
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

static void gb_sdl_handle_button(struct gb *gb, unsigned button, bool pressed) {
     /* A and B are swapped between the GB and SDL (XBox) conventions */
     switch (button) {
     case SDL_CONTROLLER_BUTTON_START:
          gb_input_set(gb, GB_INPUT_START, pressed);
          break;
     case SDL_CONTROLLER_BUTTON_BACK:
          gb_input_set(gb, GB_INPUT_SELECT, pressed);
          break;
     case SDL_CONTROLLER_BUTTON_B:
          gb_input_set(gb, GB_INPUT_A, pressed);
          break;
     case SDL_CONTROLLER_BUTTON_A:
          gb_input_set(gb, GB_INPUT_B, pressed);
          break;
     case SDL_CONTROLLER_BUTTON_DPAD_UP:
          gb_input_set(gb, GB_INPUT_UP, pressed);
          break;
     case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
          gb_input_set(gb, GB_INPUT_DOWN, pressed);
          break;
     case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
          gb_input_set(gb, GB_INPUT_LEFT, pressed);
          break;
     case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
          gb_input_set(gb, GB_INPUT_RIGHT, pressed);
          break;
     }
}

static void gb_sdl_handle_new_controller(struct gb *gb, unsigned index) {
     struct gb_sdl_context *ctx = gb->frontend.data;

     if (ctx->controller) {
          /* We already have a controller */
          return;
     }

     if (!SDL_IsGameController(index)) {
          return;
     }

     ctx->controller = SDL_GameControllerOpen(index);
     if (ctx->controller != NULL) {
          printf("Using controller '%s'\n",
                 SDL_GameControllerName(ctx->controller));
     }
}

static void gb_sdl_find_controller(struct gb *gb) {
     struct gb_sdl_context *ctx = gb->frontend.data;
     unsigned i;

     for (i = 0; ctx->controller == NULL && i < SDL_NumJoysticks(); i++) {
          gb_sdl_handle_new_controller(gb, i);
     }

     if (ctx->controller == NULL) {
          printf("No controller found\n");
     }
}

static void gb_sdl_handle_controller_removed(struct gb *gb, Sint32 which) {
     struct gb_sdl_context *ctx = gb->frontend.data;
     SDL_Joystick *joy;

     if (!ctx->controller) {
          return;
     }

     joy = SDL_GameControllerGetJoystick(ctx->controller);
     if (!joy) {
          return;
     }

     if (SDL_JoystickInstanceID(joy) == which) {
          /* the controller we were using has been removed */
          printf("Controller removed\n");
          SDL_GameControllerClose(ctx->controller);
          ctx->controller = NULL;

          /* Attempt to find a replacement */
          gb_sdl_find_controller(gb);
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
          case SDL_CONTROLLERBUTTONDOWN:
          case SDL_CONTROLLERBUTTONUP:
               gb_sdl_handle_button(gb, e.cbutton.button,
                                    e.cbutton.state == SDL_PRESSED);
               break;
          case SDL_CONTROLLERDEVICEREMOVED:
               gb_sdl_handle_controller_removed(gb, e.cdevice.which);
               break;
          case SDL_CONTROLLERDEVICEADDED:
               gb_sdl_handle_new_controller(gb, e.cdevice.which);
               break;
          }
     }
}

static void gb_sdl_flip(struct gb *gb) {
     struct gb_sdl_context *ctx = gb->frontend.data;

     /* Copy pixels to the canvas texture */
     SDL_UpdateTexture(ctx->canvas, NULL, ctx->pixels,
                       GB_LCD_WIDTH * UPSCALE_FACTOR * sizeof(ctx->pixels[0]));

     /* Render the canvas */
     SDL_RenderCopy(ctx->renderer, ctx->canvas, NULL, NULL);
     SDL_RenderPresent(ctx->renderer);
}

static void gb_sdl_destroy(struct gb *gb) {
     struct gb_sdl_context *ctx = gb->frontend.data;

     if (ctx->controller) {
          SDL_GameControllerClose(ctx->controller);
     }

     SDL_DestroyTexture(ctx->canvas);
     SDL_DestroyRenderer(ctx->renderer);
     SDL_DestroyWindow(ctx->window);
     SDL_Quit();
     free(ctx);
     gb->frontend.data = NULL;
}

static void gb_sdl_audio_callback(void *userdata,
                                  Uint8 *stream,
                                  int len) {
     struct gb *gb = userdata;
     struct gb_sdl_context *ctx = gb->frontend.data;
     struct gb_spu_sample_buffer *buf = &gb->spu.buffers[ctx->audio_buf_index];

     /* Normally the frontend should always request exactly the correct length
      */
     assert(len == sizeof(buf->samples));

     if (sem_trywait(&buf->ready) == 0) {
          /* Buffer is ready */
          memcpy(stream, buf->samples, sizeof(buf->samples));

          /* Tell the SPU that it can refill this buffer */
          sem_post(&buf->free);
          /* Move on to the next buffer */
          ctx->audio_buf_index = (ctx->audio_buf_index + 1)
               % GB_SPU_SAMPLE_BUFFER_COUNT;
     } else {
          /* Buffer is not ready yet, we're running slow! */
          fprintf(stderr, "Emulator is running too slow!\n");
          memset(stream, 0, sizeof(buf->samples));
     }
}

void gb_sdl_frontend_init(struct gb *gb) {
     struct gb_sdl_context *ctx;
     SDL_AudioSpec want;

     ctx = malloc(sizeof(*ctx));
     if (ctx == NULL) {
          perror("Malloc failed");
          die();
     }

     gb->frontend.data = ctx;

     ctx->audio_buf_index = 0;

     if (SDL_Init(SDL_INIT_VIDEO |
                  SDL_INIT_GAMECONTROLLER |
                  SDL_INIT_AUDIO) < 0) {
          fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
          die();
     }

     if (SDL_CreateWindowAndRenderer(GB_LCD_WIDTH * UPSCALE_FACTOR, GB_LCD_HEIGHT * UPSCALE_FACTOR,
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

     SDL_memset(&want, 0, sizeof(want));
     want.freq = GB_SPU_SAMPLE_RATE_HZ;
     want.format = AUDIO_S16SYS;
     want.channels = 2;
     want.samples = GB_SPU_SAMPLE_BUFFER_LENGTH;
     want.callback = gb_sdl_audio_callback;
     want.userdata = gb;

     ctx->audio_device = SDL_OpenAudioDevice(NULL, 0,
                                             &want, &ctx->audio_spec,
                                             0 /* No changes allowed */);
     if (ctx->audio_device == 0) {
          fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
          die();
     }

     /* Start audio */
     SDL_PauseAudioDevice(ctx->audio_device, 0);

     gb->frontend.draw_line_dmg = gb_sdl_draw_line_dmg;
     gb->frontend.draw_line_gbc = gb_sdl_draw_line_gbc;
     gb->frontend.flip = gb_sdl_flip;
     gb->frontend.refresh_input = gb_sdl_refresh_input;
     gb->frontend.destroy = gb_sdl_destroy;

     /* Clear the canvas */
     memset(ctx->pixels, 0, sizeof(ctx->pixels));
     gb_sdl_flip(gb);

     ctx->controller = NULL;
     gb_sdl_find_controller(gb);
}
