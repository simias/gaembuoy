#include <stdio.h>
#include "gb.h"

/* ROM (bank 0 + 1) */
#define ROM_BASE        0x0000U
#define ROM_END         (ROM_BASE + 0x8000U)
/* Zero page RAM */
#define ZRAM_BASE       0xff80U
#define ZRAM_END        (ZRAM_BASE + 0x7fU)

/* Read one byte from memory at `addr` */
uint8_t gb_memory_readb(struct gb *gb, uint16_t addr) {
     if (addr >= ROM_BASE && addr < ROM_END) {
          return gb_cart_rom_readb(gb, addr - ROM_BASE);
     }

     printf("Unsupported read at address 0x%04x\n", addr);
     die();

     return 0xff;
}

void gb_memory_writeb(struct gb *gb, uint16_t addr, uint8_t val) {

     if (addr >= ZRAM_BASE && addr < ZRAM_END) {
          gb->zram[addr - ZRAM_BASE] = val;
          return;
     }

     printf("Unsupported write at address 0x%04x [val=0x%02x]\n", addr, val);
     die();
}
