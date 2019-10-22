#include <string.h>
#include <stdio.h>
#include "gb.h"
#include "sdl.h"

int main(int argc, char **argv) {
     struct gb *gb;
     const char *rom_file;
     unsigned i;

     if (argc < 2) {
          fprintf(stderr, "Usage: %s <rom>\n", argv[0]);
          return EXIT_FAILURE;
     }

     /* Our context contains semaphores, so we allocate it on the heap so that
      * it remains visible to all threads no matter what. */
     gb = calloc(1, sizeof(*gb));
     if (gb == NULL) {
          perror("calloc failed");
          return EXIT_FAILURE;
     }

     /* Initialize the semaphores before we start the frontend */
     for (i = 0; i < GB_SPU_SAMPLE_BUFFER_COUNT; i++) {
          struct gb_spu_sample_buffer *buf = &gb->spu.buffers[i];

          memset(buf->samples, 0, sizeof(buf->samples));

          /* We initialize the buffers by saying that they're ready to be sent
           * to the frontend. This way the frontend won't starve for audio while
           * we start the emulation. */
          sem_init(&buf->free, 0, 0);
          sem_init(&buf->ready, 0, 1);
     }

     gb_sdl_frontend_init(gb);

     rom_file = argv[1];

     gb_cart_load(gb, rom_file);
     gb_sync_reset(gb);
     gb_irq_reset(gb);
     gb_cpu_reset(gb);
     gb_gpu_reset(gb);
     gb_input_reset(gb);
     gb_dma_reset(gb);
     gb_timer_reset(gb);
     gb_spu_reset(gb);

     gb->iram_high_bank = 1;
     gb->vram_high_bank = false;
     gb->quit = false;
     gb->double_speed = false;
     gb->speed_switch_pending = false;

     while (!gb->quit) {
          gb->frontend.refresh_input(gb);

          /* We refresh the input at 120Hz. This is a trade-off, if we refresh
           * faster we'll reduce latency at the cost of performance. */
          gb_cpu_run_cycles(gb, GB_CPU_FREQ_HZ / 120);
     }

     gb->frontend.destroy(gb);
     gb_cart_unload(gb);

     free(gb);

     return 0;
}
