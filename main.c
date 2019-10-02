#include <string.h>
#include <stdio.h>
#include "gb.h"
#include "sdl.h"

int main(int argc, char **argv) {
     struct gb gb;
     const char *rom_file;

     memset(&gb, 0, sizeof(gb));

     if (argc < 2) {
          fprintf(stderr, "Usage: %s <rom>\n", argv[0]);
          return EXIT_FAILURE;
     }

     gb_sdl_frontend_init(&gb);

     rom_file = argv[1];

     gb_cart_load(&gb, rom_file);
     gb_sync_reset(&gb);
     gb_cpu_reset(&gb);
     gb_gpu_reset(&gb);
     gb_input_reset(&gb);

     gb.quit = false;
     while (!gb.quit) {
          /* Run emulator core until a frame is rendered */
          gb_cpu_run_frame(&gb);

          gb.frontend.flip(&gb);
          gb.frontend.refresh_input(&gb);
          gb_sync_rebase(&gb);
     }

     gb.frontend.destroy(&gb);
     gb_cart_unload(&gb);

     return 0;
}
