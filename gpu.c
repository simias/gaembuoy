#include <stdio.h>
#include "gb.h"

void gb_gpu_set_lcd_stat(struct gb *gb, uint8_t stat) {
     struct gb_gpu *gpu = &gb->gpu;

     gpu->iten_mode0 = stat & 0x8;
     gpu->iten_mode1 = stat & 0x10;
     gpu->iten_mode2 = stat & 0x20;
     gpu->iten_lyc   = stat & 0x40;

     fprintf(stderr,
             "GPU ITEN: mode0: %d, mode1: %d, mode2: %d, lyc: %d\n",
             gpu->iten_mode0,
             gpu->iten_mode1,
             gpu->iten_mode2,
             gpu->iten_lyc);
}

void gb_gpu_set_lcdc(struct gb *gb, uint8_t ctrl) {
     gb->gpu.lcdc = ctrl;
     fprintf(stderr, "GPU LCDC: 0x%02x\n", ctrl);
}

uint8_t gb_gpu_get_lcdc(struct gb *gb) {
     return gb->gpu.lcdc;
}
