#include <stdio.h>
#include "gb.h"

/* GPU timings:
 *
 * - One line:
 *      | Mode 2: 80 cycles | Mode 3: 172 cycles | Mode 0: 204 cycles |
 *   Total: 456 cycles
 *
 * - We draw each line at the boundary between Mode 3 and Mode 0 (not very
 *   accurate, but simple and works well enough)
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
     unsigned i;

     gpu->scx = 0;
     gpu->scy = 0;
     gpu->iten_lyc = false;
     gpu->iten_mode0 = false;
     gpu->iten_mode1 = false;
     gpu->iten_mode2 = false;
     gpu->master_enable = true;
     gpu->bg_enable = false;
     gpu->window_enable = false;
     gpu->sprite_enable = false;
     gpu->tall_sprites = false;
     gpu->bg_use_high_tm = false;
     gpu->window_use_high_tm = false;
     gpu->bg_window_use_sprite_ts = false;
     gpu->ly = 0;
     gpu->bgp = 0;
     gpu->obp0 = 0;
     gpu->obp1 = 0;
     gpu->line_pos = 0;

     for (i = 0; i < sizeof(gpu->oam); i++) {
          gpu->oam[i] = 0;
     }
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

struct gb_gpu_pixel {
     enum gb_color color;
     bool opaque;
};

static enum gb_color gb_gpu_get_tile_color(struct gb *gb,
                                           uint8_t tile_index,
                                           uint8_t x, uint8_t y,
                                           bool use_sprite_ts) {
     unsigned tile_addr;
     /* Each tile is 8x8 pixels and stores 2bits per pixels for a total of
      * 16bytes per tile */
     const unsigned tile_size = 16;
     unsigned lsb;
     unsigned msb;

     if (use_sprite_ts) {
          /* Sprite tile set starts at the beginning of VRAM */
          tile_addr = tile_index * tile_size;
     } else {
          /* The other tile set (which can optionally be used by the background
           * and window) starts just after the sprite tile set but there's a
           * trick: the tile index is used as a *signed* value, which means that
           * values above 127 index *back* into the second half of the sprite
           * tile set, effectively sharing the region between the two sets */
          tile_addr = 0x1000 + (int8_t)tile_index * tile_size;
     }

     /* Pixel data is stored "backwards" in VRAM: the leftmost pixel (x = 0) is
      * stored in the MSB (byte >> 7) */
     x = 7 - x;

     /* The pixel value is two bits split across two contiguous bytes */
     lsb = (gb->vram[tile_addr + y * 2 + 0] >> x) & 1;
     msb = (gb->vram[tile_addr + y * 2 + 1] >> x) & 1;

     return (msb << 1) | lsb;
}

static enum gb_color gb_gpu_palette_transform(enum gb_color color,
                                              uint8_t palette) {
     unsigned off = 2 * color;

     return (palette >> off) & 3;
}

static struct gb_gpu_pixel gb_gpu_get_bg_win_pixel(struct gb *gb,
                                                   uint8_t x, uint8_t y,
                                                   bool use_high_tm) {
     struct gb_gpu *gpu = &gb->gpu;

     /* Coordinates of the tile in the tile map (each tile is 8x8 pixels) */
     unsigned tile_map_x = x / 8;
     unsigned tile_map_y = y / 8;
     /* Coordinates of the pixel within the tile */
     unsigned tile_x = x % 8;
     unsigned tile_y = y % 8;
     /* Offset of the tile map entry in the VRAM */
     unsigned tm_addr;
     /* Index of the tile entry in the tile set */
     uint8_t tile_index;
     struct gb_gpu_pixel pix;

     /* There are two independent tile maps the game can use */
     if (use_high_tm) {
          tm_addr = 0x1c00;
     } else {
          tm_addr = 0x1800;
     }

     /* The tile map is a square map of 32*32 tiles. For each tile it contains
      * one byte (8bits) which is an index in the tile set. */
     tm_addr += tile_map_y * 32 + tile_map_x;

     /* Look up the tile map entry in VRAM */
     tile_index = gb->vram[tm_addr];

     pix.color = gb_gpu_get_tile_color(gb, tile_index, tile_x, tile_y,
                                       gpu->bg_window_use_sprite_ts);
     pix.opaque = pix.color != GB_COL_WHITE;

     pix.color = gb_gpu_palette_transform(pix.color, gpu->bgp);

     return pix;
}

static struct gb_gpu_pixel gb_gpu_get_bg_pixel(struct gb *gb,
                                               unsigned x, unsigned y) {
     struct gb_gpu *gpu = &gb->gpu;
     uint8_t bgx = (x + gpu->scx) & 0xff;
     uint8_t bgy = (y + gpu->scy) & 0xff;

     return gb_gpu_get_bg_win_pixel(gb, bgx, bgy, gpu->bg_use_high_tm);
}

static void gb_gpu_draw_cur_line(struct gb *gb) {
     struct gb_gpu *gpu = &gb->gpu;
     enum gb_color line[GB_LCD_WIDTH];
     unsigned x;

     for (x = 0; x < GB_LCD_WIDTH; x++) {
          struct gb_gpu_pixel p = {
               .color = GB_COL_WHITE,
               .opaque = false
          };

          if (gpu->master_enable) {
               if (gpu->bg_enable) {
                    p = gb_gpu_get_bg_pixel(gb, x, gpu->ly);
               }
          }

          line[x] = p.color;
     }

     gb->frontend.draw_line(gb, gpu->ly, line);
}

void gb_gpu_sync(struct gb *gb) {
     struct gb_gpu *gpu = &gb->gpu;
     int32_t elapsed = gb_sync_resync(gb, GB_SYNC_GPU);
     /* Number of cycles needed to finish the current line */
     uint16_t line_remaining = HTOTAL - gpu->line_pos;

     while (elapsed > 0) {
          uint8_t prev_mode = gb_gpu_get_mode(gb);

          if (elapsed < line_remaining) {
               /* Current line not finished */
               gpu->line_pos += elapsed;
               line_remaining -= elapsed;
               elapsed = 0;

               if (prev_mode != 0 && gb_gpu_get_mode(gb) == 0) {
                    /* We didn't finish the line but we did cross the Mode 3 ->
                     * Mode 0 boundary, draw the current line */
                    gb_gpu_draw_cur_line(gb);
               }
          } else {
               /* We reached the end of this line */
               elapsed -= line_remaining;

               if (prev_mode == 2 || prev_mode == 3) {
                    /* We're about to finish the current line but we hadn't
                     * reached the Mode 0 boundary yet, which means that we
                     * still have to draw it */
                    gb_gpu_draw_cur_line(gb);
               }

               /* Move on to the next line */
               gpu->ly++;
               gpu->line_pos = 0;
               line_remaining = HTOTAL;

               if (gpu->ly == VSYNC_START) {
                    /* We're done drawing the current frame */
                    gb->frame_done = true;
               }

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

     if (!gpu->master_enable) {
          return 0;
     }

     gb_gpu_sync(gb);

     r |= gb_gpu_get_mode(gb);
     r |= gpu->iten_mode0 << 3;
     r |= gpu->iten_mode1 << 4;
     r |= gpu->iten_mode2 << 5;
     r |= gpu->iten_lyc << 6;

     /* XXX: Set LYC coincidence */

     return r;
}

void gb_gpu_set_lcdc(struct gb *gb, uint8_t lcdc) {
     struct gb_gpu *gpu = &gb->gpu;
     bool master_enable;

     gb_gpu_sync(gb);

     gpu->bg_enable = lcdc & 0x01;
     gpu->sprite_enable = lcdc & 0x02;
     gpu->tall_sprites = lcdc & 0x04;
     gpu->bg_use_high_tm = lcdc & 0x08;
     gpu->bg_window_use_sprite_ts = lcdc & 0x10;
     gpu->window_enable = lcdc & 0x20;
     gpu->window_use_high_tm = lcdc & 0x40;
     master_enable = lcdc & 0x80;

     if (master_enable != gpu->master_enable) {
          gpu->master_enable = master_enable;

          if (master_enable) {
               /* GPU was just re-enabled, restart from the beginning of the
                * first line */
               gpu->ly = 0;
               gpu->line_pos = 0;
               gb_gpu_sync(gb);
          }
     }
}

uint8_t gb_gpu_get_lcdc(struct gb *gb) {
     struct gb_gpu *gpu = &gb->gpu;
     uint8_t lcdc = 0;

     gb_gpu_sync(gb);

     lcdc |= (gpu->bg_enable << 0);
     lcdc |= (gpu->sprite_enable << 1);
     lcdc |= (gpu->tall_sprites << 2);
     lcdc |= (gpu->bg_use_high_tm << 3);
     lcdc |= (gpu->bg_window_use_sprite_ts << 4);
     lcdc |= (gpu->window_enable << 5);
     lcdc |= (gpu->window_use_high_tm << 6);
     lcdc |= (gpu->master_enable << 7);

     return lcdc;
}

uint8_t gb_gpu_get_ly(struct gb *gb) {
     struct gb_gpu *gpu = &gb->gpu;

     if (!gpu->master_enable) {
          return 0;
     }

     gb_gpu_sync(gb);

     return gpu->ly;
}
