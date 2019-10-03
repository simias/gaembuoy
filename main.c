#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include "gb.h"
#include "sdl.h"

void gb_get_time(struct timespec *ts) {
     if (clock_gettime(CLOCK_MONOTONIC, ts) < 0) {
          perror("clock_gettime failed");
          die();
     }
}

void gb_sync_to_realtime(int32_t cycles_since_ref, struct timespec ref_date) {
     struct timespec now;
     struct timespec target;
     struct timespec delay;
     float gb_elapsed_seconds;

     /* Compute how much real time a GameBoy would take to run
      * for cycles_since_ref. Use floating point arithmetics to avoid
      * overflows */
     gb_elapsed_seconds = (float)cycles_since_ref / GB_CPU_FREQ_HZ;

     /* Add this delay to the ref_date to figure out what should be the
      * current date if we want to run in real time */
     target = ref_date;
     target.tv_sec += (int)gb_elapsed_seconds;
     target.tv_nsec +=
          (gb_elapsed_seconds - (int)gb_elapsed_seconds) * 1000000000.;
     if (target.tv_nsec >= 1000000000UL) {
          target.tv_sec++;
          target.tv_nsec -= 1000000000UL;
     }

     /* Now we need to figure out how long we need to sleep to reach the target
      * date */
     gb_get_time(&now);

     if (now.tv_sec > target.tv_sec ||
         (now.tv_sec == target.tv_sec && now.tv_nsec >= target.tv_nsec)) {
          fprintf(stderr, "Emulator is running too slow!\n");
          /* TODO: go back in time */
          return;
     }

     /* To hit the target we just need to sleep for the difference between now
      * and target */
     delay.tv_sec = target.tv_sec - now.tv_sec;
     if (target.tv_nsec >= now.tv_nsec) {
          delay.tv_nsec = target.tv_nsec - now.tv_nsec;
     } else {
          delay.tv_sec--;
          delay.tv_nsec = (target.tv_nsec + 1000000000UL) - now.tv_nsec;
     }

     while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {
          /* Sleep was interrupted before it was finished, loop */
          ;
     }
}

int main(int argc, char **argv) {
     struct gb gb;
     const char *rom_file;
     struct timespec ref_date;

     memset(&gb, 0, sizeof(gb));

     if (argc < 2) {
          fprintf(stderr, "Usage: %s <rom>\n", argv[0]);
          return EXIT_FAILURE;
     }

     gb_sdl_frontend_init(&gb);

     rom_file = argv[1];

     gb_cart_load(&gb, rom_file);
     gb_sync_reset(&gb);
     gb_irq_reset(&gb);
     gb_cpu_reset(&gb);
     gb_gpu_reset(&gb);
     gb_input_reset(&gb);

     gb.quit = false;
     while (!gb.quit) {
          int32_t elapsed_cycles;

          gb_get_time(&ref_date);

          gb.frontend.refresh_input(&gb);

          /* We refresh the display and input at 120Hz. This is a trade-off, if
           * we refresh faster we'll reduce latency at the cost of performance.
           */
          elapsed_cycles = gb_cpu_run_cycles(&gb, GB_CPU_FREQ_HZ / 120);

          if (gb.frame_done) {
               /* The GPU has rendered a new frame */
               gb.frontend.flip(&gb);
               gb.frame_done = false;
          }

          gb_sync_to_realtime(elapsed_cycles, ref_date);
     }

     gb.frontend.destroy(&gb);
     gb_cart_unload(&gb);

     return 0;
}
