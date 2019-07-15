#include <string.h>
#include <stdio.h>
#include "gb.h"

int main(int argc, char **argv) {
     struct gb gb;
     const char *rom_file;

     memset(&gb, 0, sizeof(gb));

     if (argc < 2) {
          fprintf(stderr, "Usage: %s <rom>\n", argv[0]);
          return EXIT_FAILURE;
     }

     rom_file = argv[1];

     gb_cart_load(&gb, rom_file);
     gb_cpu_reset(&gb);

     while (1) {
          gb_cpu_run_instruction(&gb);
     }

     gb_cart_unload(&gb);
     return 0;
}
