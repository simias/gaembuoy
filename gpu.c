#include <stdio.h>
#include "gb.h"

/* GPU timings:
 *
 * - One line:
 *      | Mode 2: 80 cycles | Mode 3: 172 cycles | Mode 0: 204 cycles |
 *   Total: 456 cycles
 *
 * - One frame:
 *      | Active video (Modes 2/3/0): 144 lines |
 *      | VSYNC (Mode 1): 10 lines              |
 *   Total: 154 lines (70224 cycles)
 */

/* Number of clock cycles spent in Mode 2 (OAM in use) */
#define MODE_2_CYCLES 80U
/* Number of clock cycles spent in Mode 3 (OAM + display RAM in use) */
#define MODE_3_CYCLES 172U
#define MODE_3_END    (MODE_2_CYCLES + MODE_3_CYCLES)
/* Number of clock cycles spent in Mode 0 (HSYNC) */
#define MODE_0_CYCLES 204U
/* Total number of cycles per line */
#define HTOTAL (MODE_2_CYCLES + MODE_3_CYCLES + MODE_0_CYCLES)

/* First line of the vertical blanking */
#define VSYNC_START 144U
/* Number of lines spent in vertical blanking */
#define VSYNC_LINES 10U
/* Total number of lines (including vertical blanking) */
#define VTOTAL (VSYNC_START + VSYNC_LINES)

void gb_gpu_reset(struct gb *gb) {
     struct gb_gpu *gpu = &gb->gpu;

     gpu->scx = 0;
     gpu->scy = 0;
     gpu->iten_lyc = false;
     gpu->iten_mode0 = false;
     gpu->iten_mode1 = false;
     gpu->iten_mode2 = false;
     gpu->lcdc = 0;
     gpu->ly = 0;
     gpu->bgp = 0;
     gpu->obp0 = 0;
     gpu->obp1 = 0;
     gpu->line_pos = 0;
}

static uint8_t gb_gpu_get_mode(struct gb *gb) {
     struct gb_gpu *gpu = &gb->gpu;

     if (gpu->ly >= VSYNC_START) {
          /* Mode 1: VBLANK */
          return 1;
     }

     if (gpu->line_pos < MODE_2_CYCLES) {
          /* Mode 2: OAM access */
          return 2;
     }

     if (gpu->line_pos < MODE_3_END) {
          /* Mode 3: OAM + display RAM in use */
          return 3;
     }

     /* Mode 0: horizontal blanking */
     return 0;
}

void gb_gpu_sync(struct gb *gb) {
     struct gb_gpu *gpu = &gb->gpu;
     int32_t elapsed = gb_sync_resync(gb, GB_SYNC_GPU);
     /* Number of cycles needed to finish the current line */
     uint16_t line_remaining = HTOTAL - gpu->line_pos;

     while (elapsed > 0) {
          if (elapsed < line_remaining) {
               /* Current line not finished */
               gpu->line_pos += elapsed;
               line_remaining -= elapsed;
               elapsed = 0;
          } else {
               /* We reached the end of this line */
               elapsed -= line_remaining;

               /* Move on to the next line */
               gpu->ly++;
               gpu->line_pos = 0;
               line_remaining = HTOTAL;

               if (gpu->ly >= VTOTAL) {
                    /* Move on to the next frame */
                    gpu->ly = 0;
               }
          }
     }

     /* Force a sync at the beginning of the next line */
     gb_sync_next(gb, GB_SYNC_GPU, line_remaining);
}

void gb_gpu_set_lcd_stat(struct gb *gb, uint8_t stat) {
     struct gb_gpu *gpu = &gb->gpu;

     gb_gpu_sync(gb);

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

uint8_t gb_gpu_get_lcd_stat(struct gb *gb) {
     struct gb_gpu *gpu = &gb->gpu;
     uint8_t r = 0;

     gb_gpu_sync(gb);

     r |= gb_gpu_get_mode(gb);
     r |= gpu->iten_mode0 << 3;
     r |= gpu->iten_mode1 << 4;
     r |= gpu->iten_mode2 << 5;
     r |= gpu->iten_lyc << 6;

     /* XXX: Set LYC coincidence */

     return r;
}

void gb_gpu_set_lcdc(struct gb *gb, uint8_t ctrl) {

     gb_gpu_sync(gb);

     gb->gpu.lcdc = ctrl;
     fprintf(stderr, "GPU LCDC: 0x%02x\n", ctrl);
}

uint8_t gb_gpu_get_lcdc(struct gb *gb) {

     gb_gpu_sync(gb);

     return gb->gpu.lcdc;
}

uint8_t gb_gpu_get_ly(struct gb *gb) {
     struct gb_gpu *gpu = &gb->gpu;

     gb_gpu_sync(gb);

     return gpu->ly;
}
