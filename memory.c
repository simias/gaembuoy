#include <stdio.h>
#include "gb.h"

/* ROM (bank 0 + 1) */
#define ROM_BASE        0x0000U
#define ROM_END         (ROM_BASE + 0x8000U)

/* Read one byte from memory at `addr` */
uint8_t gb_memory_readb(struct gb *gb, uint16_t addr) {
     if (addr >= ROM_BASE && addr < ROM_END) {
          return gb_cart_rom_readb(gb, addr - ROM_BASE);
     }

     printf("Unsupported read at address 0x%04x\n", addr);
     die();

     return 0xff;
}
