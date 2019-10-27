#include "gb.h"

static void gb_hdma_copy(struct gb *gb, uint16_t len) {
     struct gb_hdma *hdma = &gb->hdma;
     uint16_t src = hdma->source;
     uint16_t dst = hdma->destination;

     /* Copy takes about 2 cycles per byte */
     gb->timestamp += len * 2;

     while (len--) {
          uint16_t vram_addr;
          uint8_t v;

          /* Destination has to be in VRAM */
          vram_addr = 0x8000U + (dst % 0x2000U);

          v = gb_memory_readb(gb, src);
          gb_memory_writeb(gb, vram_addr, v);

          src++;
          dst++;
     }

     hdma->source = src;
     hdma->destination = dst;
}

void gb_hdma_start(struct gb *gb, bool hblank) {
     struct gb_hdma *hdma = &gb->hdma;

     /* XXX TODO: for now assume we always run at once */
     hblank = false;

     if (hblank) {
          /* TODO */
     } else {
          /* Do the transfer in one shot */
          uint16_t len = hdma->length;

          len = (len + 1) * 0x10;

          gb_hdma_copy(gb, len);

          /* Transfer done */
          hdma->run_on_hblank = false;
          hdma->length = 0x7f;
     }
}
