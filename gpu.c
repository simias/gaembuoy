#include <stdio.h>
#include "gb.h"

/* GPU timings:
 *
 * - One line:
 *      | Mode 2: 80 cycles | Mode 3: 172 cycles | Mode 0: 204 cycles |
 *   Total: 456 cycles
 *
 *   Mode 2: OAM in use
 *   Mode 3: OAM and VRAM in use
 *   Mode 0: Horizontal blanking (CPU can access OAM and VRAM)
 *
 * - We draw each line at the boundary between Mode 3 and Mode 0 (not very
 *   accurate, but simple and works well enough)
 *
 * - One frame:
 *      | Active video (Modes 2/3/0): 144 lines |
 *      | VSYNC (Mode 1): 10 lines              |
 *   Total: 154 lines (70224 cycles)
 *
 *   Mode 1: Vertical blanking (CPU can access OAM and VRAM)
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
     gpu->lyc = 0;
     gpu->bgp = 0;
     gpu->obp0 = 0;
     gpu->obp1 = 0;
     gpu->wx = 0;
     gpu->wy = 0;
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
     union gb_gpu_color color;
     bool opaque;
     /* GBC only: true if the background pixel has priority */
     bool priority;
};

/* Get a pixel value from the tileset */
static enum gb_color gb_gpu_get_tile_color(struct gb *gb,
                                           uint8_t tile_index,
                                           uint8_t x, uint8_t y,
                                           bool use_sprite_ts,
                                           bool use_high_bank) {
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

     /* GBC-only: use the high bank if requested */
     if (use_high_bank) {
          tile_addr += 0x2000;
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
     bool use_sprite_ts = gpu->bg_window_use_sprite_ts;

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

     if (gb->gbc) {
          /* On the GBC we have additional attributes in the 2nd VRAM bank */
          uint8_t attrs = gb->vram[tm_addr + 0x2000];
          bool priority = attrs & 0x80;
          bool y_flip = attrs & 0x40;
          bool x_flip = attrs & 0x20;
          bool high_bank = attrs & 0x08;
          uint8_t palette = attrs & 0x07;
          enum gb_color col;

          pix.priority = priority;

          if (x_flip) {
               tile_x = 7 - tile_x;
          }

          if (y_flip) {
               tile_y = 7 - tile_y;
          }

          col = gb_gpu_get_tile_color(gb, tile_index,
                                      tile_x, tile_y,
                                      use_sprite_ts,
                                      high_bank);

          pix.opaque = col != GB_COL_WHITE;

          pix.color.gbc_color = gpu->bg_palettes.colors[palette][col];
     } else {
          pix.priority = false;

          pix.color.dmg_color = gb_gpu_get_tile_color(gb, tile_index,
                                                      tile_x, tile_y,
                                                      use_sprite_ts,
                                                      false);
          pix.opaque = pix.color.dmg_color != GB_COL_WHITE;

          pix.color.dmg_color = gb_gpu_palette_transform(pix.color.dmg_color,
                                                         gpu->bgp);
     }

     return pix;
}

static struct gb_gpu_pixel gb_gpu_get_bg_pixel(struct gb *gb,
                                               unsigned x, unsigned y) {
     struct gb_gpu *gpu = &gb->gpu;
     uint8_t bgx = (x + gpu->scx) & 0xff;
     uint8_t bgy = (y + gpu->scy) & 0xff;

     return gb_gpu_get_bg_win_pixel(gb, bgx, bgy, gpu->bg_use_high_tm);
}

static struct gb_gpu_pixel gb_gpu_get_win_pixel(struct gb *gb,
                                                unsigned x, unsigned y) {
     struct gb_gpu *gpu = &gb->gpu;
     uint8_t wx = x + 7 - gpu->wx;
     uint8_t wy = y - gpu->wy;

     return gb_gpu_get_bg_win_pixel(gb, wx, wy, gpu->window_use_high_tm);
}

struct gb_sprite {
     /* Coordinates of the sprite's top-left corner */
     int x;
     int y;

     /* Index of the sprite's pixel data in the sprite tile set. 8x16 sprites
      * use two consecutive tiles */
     uint8_t tile_index;

     /* True if the sprite must be displayed behind the background (that is,
      * only visible if the background is disabled or through transparent
      * pixels) */
     bool background;

     /* True if the sprite is flipped horizontally or vertically */
     bool x_flip;
     bool y_flip;

     /* GB-only: true if sprite uses palette obp1, otherwise use obp0 */
     bool use_obp1;

     /* GBC-only: if true the tile is in the high bank */
     bool high_bank;

     /* GBC-only: which palette to use */
     uint8_t palette;
};

static struct gb_sprite gb_get_oam_sprite(struct gb *gb, unsigned index) {
     struct gb_gpu *gpu = &gb->gpu;
     struct gb_sprite s;
     unsigned oam_off = index * 4;
     uint8_t flags;

     /* Y coordinates have an offset of 16 (so that they can clip at the top of
      * the screen) */
     s.y = (int)gpu->oam[oam_off] - 16;

     /* X coordinates have an offset of 8 (so that they can clip to the left of
      * the screen) */
     s.x = (int)gpu->oam[oam_off + 1] - 8;

     s.tile_index = gpu->oam[oam_off + 2];

     flags = gpu->oam[oam_off + 3];

     s.use_obp1 = flags & 0x10;
     s.x_flip = flags & 0x20;
     s.y_flip = flags & 0x40;
     s.background = flags & 0x80;

     if (gb->gbc) {
          s.high_bank = flags & 0x08;
          s.palette = flags & 0x07;
     } else {
          s.high_bank = false;
          s.palette = 0;
     }

     return s;
}

/* Max number of sprites per line */
#define GB_GPU_LINE_SPRITES 10

static void gb_gpu_get_line_sprites(
     struct gb *gb,
     unsigned ly,
     struct gb_sprite sprites[GB_GPU_LINE_SPRITES + 1]) {

     struct gb_gpu *gpu = &gb->gpu;
     int i;
     unsigned n_sprites;
     unsigned sprite_height;

     if (!gpu->sprite_enable) {
          /* Sprites are disabled, mark the end of the list with an out-of-frame
           * sprite and bail out */
          sprites[0].x = GB_LCD_WIDTH * 2;
          return;
     }

     if (gpu->tall_sprites) {
          sprite_height = 16;
     } else {
          sprite_height = 8;
     }

     /* Iterate over the OAM and store the sprites that are in the current line.
      */
     n_sprites = 0;
     for (i = 0; i < GB_GPU_MAX_SPRITES; i++) {
          struct gb_sprite s = gb_get_oam_sprite(gb, i);

          if ((int)ly < s.y || (int)ly >= (s.y + (int)sprite_height)) {
               /* Sprite isn't on this line */
               continue;
          }

          sprites[n_sprites] = s;
          n_sprites++;
          if (n_sprites >= GB_GPU_LINE_SPRITES) {
               /* We reached the maximum number of sprites that can be displayed
                * on this line, ignore the rest */
               break;
          }
     }

     /* Mark the end of the sprite list with an unreachable out-of-frame sprite
      */
     sprites[n_sprites].x = GB_LCD_WIDTH * 2;

     if (gb->gbc) {
          /* In GBC mode the sprite priority is not based on X-coordinates but
           * simply on the index in OAM, so we already have the entries in the
           * array in the right order (from highest priority to lowest) */
          return;
     }

     /* Finally we need to sort the sprites by x-coordinate. Careful: if the
      * sprites have the same x-coordinates the position in OAM gives the
      * priority so we must use a stable sort to maintain the ordering of values
      * with the same x value */
     for (i = 1; i < n_sprites; i++) {
          struct gb_sprite cur = sprites[i];
          int j;

          /* We move cur back as long as we don't encounter a sprite with
           * greater-or-equal x value (or we reach the beginning of the list) */
          for (j = i - 1; j >= 0; j--) {
               if (sprites[j].x <= cur.x) {
                    break;
               }

               sprites[j + 1] = sprites[j];
          }

          sprites[j + 1] = cur;
     }
}

/* Attempt to sample the given sprite at the given location on the screen.
 * Returns false if the sprite is not visible at these coordinates, otherwise it
 * updates `p` with the pixel color and returns true. */
static bool gb_gpu_get_sprite_col(struct gb *gb,
                                  const struct gb_sprite *sprite,
                                  unsigned x,
                                  unsigned y,
                                  struct gb_gpu_pixel *p) {
     struct gb_gpu *gpu = &gb->gpu;
     unsigned sprite_x;
     unsigned sprite_y;
     unsigned sprite_flip_height;
     uint8_t tile_index;
     enum gb_color col;

     if (sprite->background && p->opaque) {
          /* Sprite is behind the background layer and the background pixel is
           * opaque so we return the background color directly */
          return false;
     }

     sprite_x = (int)x - sprite->x;
     sprite_y = (int)y - sprite->y;

     if (gpu->tall_sprites) {
          /* 8x16 sprites use two consecutive tiles. The first tile's index's
           * LSB is always assumed to be 0 */
          tile_index = sprite->tile_index & 0xfe;
          sprite_flip_height = 15;
     } else {
          tile_index = sprite->tile_index;
          sprite_flip_height = 7;
     }

     if (sprite->x_flip) {
          sprite_x = 7 - sprite_x;
     }

     if (sprite->y_flip) {
          sprite_y = sprite_flip_height - sprite_y;
     }

     col = gb_gpu_get_tile_color(gb, tile_index,
                                 sprite_x, sprite_y,
                                 true, sprite->high_bank);

     /* White pixel color (pre-palette) denotes a transparent pixel */
     if (col == GB_COL_WHITE) {
          return false;
     }

     if (gb->gbc) {
          p->color.gbc_color =
               gpu->sprite_palettes.colors[sprite->palette][col];
     } else {
          uint8_t palette;

          if (sprite->use_obp1) {
               palette = gpu->obp1;
          } else {
               palette = gpu->obp0;
          }

          p->color.dmg_color = gb_gpu_palette_transform(col, palette);
     }

     return true;
}

/* Returns true if the given screen coordinates lie within the window */
static bool gb_gpu_pix_in_window(struct gb *gb, unsigned x, unsigned y) {
     struct gb_gpu *gpu = &gb->gpu;
     int wx = (int)gpu->wx - 7;

     return (int)x >= wx && y >= gpu->wy;
}

static void gb_gpu_draw_cur_line(struct gb *gb) {
     struct gb_gpu *gpu = &gb->gpu;
     union gb_gpu_color line[GB_LCD_WIDTH];
     /* We force a "dummy" out-of-frame sprite at the end to avoid checking for
      * bounds while we draw the line */
     struct gb_sprite line_sprites[GB_GPU_LINE_SPRITES + 1];
     unsigned x;
     unsigned next_sprite = 0;

     gb_gpu_get_line_sprites(gb, gpu->ly, line_sprites);

     for (x = 0; x < GB_LCD_WIDTH; x++) {
          struct gb_gpu_pixel p = {
               .color.dmg_color = GB_COL_WHITE,
               .opaque = false,
               .priority = false,
          };
          struct gb_sprite s;
          unsigned i;

          if (gpu->window_enable && gb_gpu_pix_in_window(gb, x, gpu->ly)) {
               /* Pixel lies within the window */
               p = gb_gpu_get_win_pixel(gb, x, gpu->ly);
          } else if (gpu->bg_enable) {
               p = gb_gpu_get_bg_pixel(gb, x, gpu->ly);
          }

          /* If the background priority is set it means that the BG has the
           * priority over any sprite at this location */
          if (!p.priority || !p.opaque) {
               if (gb->gbc) {
                    /* In GBC the sprites aren't ordered by x-coordinate because
                     * the location in OAM has priority, so we have to iterate
                     * through the entire list until we find a visible sprite or
                     * we reach the end (signaled by a x value of
                     * GB_LCD_WIDTH * 2, see gb_gb_get_line_sprites) */
                    for (i = 0; line_sprites[i].x < GB_LCD_WIDTH * 2; i++) {
                         s = line_sprites[i];

                         if ((int)x < s.x || (int)x >= s.x + 8) {
                              /* Sprite is not visible at this location */
                              continue;
                         }

                         if (gb_gpu_get_sprite_col(gb, &s, x, gpu->ly, &p)) {
                              break;
                         }
                    }
               } else {
                    /* Figure out what is the next sprite we must display */
                    while (next_sprite < GB_GPU_LINE_SPRITES) {
                         if (line_sprites[next_sprite].x + 8 <= x ) {
                              /* We're done displaying this sprite */
                              next_sprite++;
                         } else {
                              /* We have not passed this sprite yet */
                              break;
                         }
                    }

                    /* Iterate on all sprites at this position until we find one
                     * that's visible or we run out */
                    for (i = next_sprite; line_sprites[i].x <= (int)x; i++) {
                         s = line_sprites[i];

                         if (gb_gpu_get_sprite_col(gb, &s, x, gpu->ly, &p)) {
                              break;
                         }
                    }
               }
          }

          line[x] = p.color;
     }

     if (gb->gbc) {
          gb->frontend.draw_line_gbc(gb, gpu->ly, line);
     } else {
          gb->frontend.draw_line_dmg(gb, gpu->ly, line);
     }
}

void gb_gpu_sync(struct gb *gb) {
     struct gb_gpu *gpu = &gb->gpu;
     struct gb_hdma *hdma = &gb->hdma;
     int32_t elapsed = gb_sync_resync(gb, GB_SYNC_GPU);
     /* Number of cycles needed to finish the current line */
     uint16_t line_remaining = HTOTAL - gpu->line_pos;
     int32_t next_event;

     if (!gpu->master_enable) {
          /* GPU isn't running */
          gb_sync_next(gb, GB_SYNC_GPU, GB_SYNC_NEVER);
          return;
     }

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

                    if (gpu->iten_mode0) {
                         gb_irq_trigger(gb, GB_IRQ_LCD_STAT);
                    }

                    if (hdma->run_on_hblank) {
                         gb_hdma_hblank(gb);
                    }
               }
          } else {
               /* We reached the end of this line */
               elapsed -= line_remaining;

               if (prev_mode == 2 || prev_mode == 3) {
                    /* We're about to finish the current line but we hadn't
                     * reached the Mode 0 boundary yet, which means that we
                     * still have to draw it */
                    gb_gpu_draw_cur_line(gb);

                    if (gpu->iten_mode0) {
                         gb_irq_trigger(gb, GB_IRQ_LCD_STAT);
                    }

                    if (hdma->run_on_hblank) {
                         gb_hdma_hblank(gb);
                    }
               }

               /* Move on to the next line */
               gpu->ly++;
               gpu->line_pos = 0;
               line_remaining = HTOTAL;

               if (gpu->ly == VSYNC_START) {
                    /* We're done drawing the current frame */
                    gb->frontend.flip(gb);
                    gb_irq_trigger(gb, GB_IRQ_VSYNC);

                    if (gpu->iten_mode1) {
                         /* We entered VSYNC, trigger the IRQ */
                         gb_irq_trigger(gb, GB_IRQ_LCD_STAT);
                    }
               }

               if (gpu->ly >= VTOTAL) {
                    /* Move on to the next frame */
                    gpu->ly = 0;
               }

               if (gpu->iten_lyc && gpu->ly == gpu->lyc) {
                    /* We reached LYC, trigger interrupt */
                    gb_irq_trigger(gb, GB_IRQ_LCD_STAT);
               }

               if (gpu->iten_mode2 && gpu->ly < VSYNC_START) {
                    /* Mode 2 is the first mode entered on a new line (outside
                     * of blanking */
                    gb_irq_trigger(gb, GB_IRQ_LCD_STAT);
               }
          }
     }

     /* By default we force a sync at the end of the current line */
     next_event = line_remaining;

     if ((gpu->iten_mode0 || hdma->run_on_hblank) && gb_gpu_get_mode(gb) >= 2) {
          /* Mode 0 IRQ has been requested or the HDMA needs to run on next
           * HBLANK and we're currently in Mode 2 or 3 (both occurring before
           * mode 0 on a line). In this case we force a synchronization before
           * the end of the line, at the start of the mode 0 sequence. If it's
           * not clear look at the comment at the top of this file describing
           * the GPU timings and the various modes. */
          next_event -= MODE_0_CYCLES;
     }

     /* Force a sync at the beginning of the next line */
     gb_sync_next(gb, GB_SYNC_GPU, next_event);
}

void gb_gpu_set_lcd_stat(struct gb *gb, uint8_t stat) {
     struct gb_gpu *gpu = &gb->gpu;
     bool prev_iten_mode0 = gpu->iten_mode0;

     gb_gpu_sync(gb);

     gpu->iten_mode0 = stat & 0x8;
     gpu->iten_mode1 = stat & 0x10;
     gpu->iten_mode2 = stat & 0x20;
     gpu->iten_lyc   = stat & 0x40;

     /* Enabling mode 0 interrupts may change the date of the next event (since
      * it occurs in the middle of the line */
     if (!prev_iten_mode0 && gpu->iten_mode0) {
          gb_gpu_sync(gb);
     }
}

uint8_t gb_gpu_get_lcd_stat(struct gb *gb) {
     struct gb_gpu *gpu = &gb->gpu;
     uint8_t r = 0;

     if (!gpu->master_enable) {
          return 0;
     }

     gb_gpu_sync(gb);

     r |= gb_gpu_get_mode(gb);
     r |= (gpu->ly == gpu->lyc) << 2;
     r |= gpu->iten_mode0 << 3;
     r |= gpu->iten_mode1 << 4;
     r |= gpu->iten_mode2 << 5;
     r |= gpu->iten_lyc << 6;

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

          if (master_enable == false) {
               union gb_gpu_color line[GB_LCD_WIDTH];
               unsigned i;

               /* Clear the screen */
               for (i = 0; i < GB_LCD_WIDTH; i++) {
                    line[i].dmg_color = GB_COL_WHITE;
               }

               for (i = 0; i < GB_LCD_HEIGHT; i++) {
                    gb->frontend.draw_line_dmg(gb, i, line);
               }

               gpu->ly = 0;
               gpu->line_pos = 0;
          }
          gb_gpu_sync(gb);
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

     gb_gpu_sync(gb);

     return gpu->ly;
}
