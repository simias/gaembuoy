#ifndef _GB_HDMA_H_
#define _GB_HDMA_H_

struct gb_hdma {
     /* Source address */
     uint16_t source;
     /* Destination offset in VRAM */
     uint16_t destination;
     /* Remaining length to copy, divided by 0x10 and decremented (so a value of
      * 0 means 0x10 bytes left, a value of 0x42 means (0x42 + 1) * 0x10 -> 670
      * bytes) */
     uint8_t length;
     /* True if we currently transfer 0x10 bytes at a time during the horizontal
      * blanking */
     bool run_on_hblank;
};

void gb_hdma_start(struct gb *gb, bool hblank);
void gb_hdma_hblank(struct gb *gb);

#endif /* _GB_HDMA_H_ */
