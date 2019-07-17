#include <stdio.h>
#include "gb.h"

/* ROM (bank 0 + 1) */
#define ROM_BASE        0x0000U
#define ROM_END         (ROM_BASE + 0x8000U)
/* Internal RAM */
#define IRAM_BASE       0xc000U
#define IRAM_END        (IRAM_BASE + 0x2000U)
/* Zero page RAM */
#define ZRAM_BASE       0xff80U
#define ZRAM_END        (ZRAM_BASE + 0x7fU)
/* Interrupt Enable register */
#define REG_IE          0xffffU

/* Read one byte from memory at `addr` */
uint8_t gb_memory_readb(struct gb *gb, uint16_t addr) {
     if (addr >= ROM_BASE && addr < ROM_END) {
          return gb_cart_rom_readb(gb, addr - ROM_BASE);
     }

     if (addr >= ZRAM_BASE && addr < ZRAM_END) {
          return gb->zram[addr - ZRAM_BASE];
     }

     if (addr >= IRAM_BASE && addr < IRAM_END) {
          return gb->iram[addr - IRAM_BASE];
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

     if (addr >= IRAM_BASE && addr < IRAM_END) {
          gb->iram[addr - IRAM_BASE] = val;
          return;
     }

     if (addr == REG_IE) {
          printf("Store IE=0x%02x\n", val);
          return;
     }

     printf("Unsupported write at address 0x%04x [val=0x%02x]\n", addr, val);
     die();
}
