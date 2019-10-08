#include <stdio.h>
#include "gb.h"

/* ROM (bank 0 + 1) */
#define ROM_BASE        0x0000U
#define ROM_END         (ROM_BASE + 0x8000U)
/* Video RAM */
#define VRAM_BASE       0x8000U
#define VRAM_END        (VRAM_BASE + 0x2000U)
/* Cartridge (generally battery-backed) RAM */
#define CRAM_BASE       0xa000U
#define CRAM_END        (CRAM_BASE + 0x2000U)
/* Internal RAM */
#define IRAM_BASE       0xc000U
#define IRAM_END        (IRAM_BASE + 0x2000U)
/* Internal RAM mirror */
#define IRAM_ECHO_BASE  0xe000U
#define IRAM_ECHO_END   (IRAM_ECHO_BASE + 0x1e00U)
/* Object Attribute Memory (sprite configuration) */
#define OAM_BASE        0xfe00U
#define OAM_END         (OAM_BASE + 0xa0U)
/* Zero page RAM */
#define ZRAM_BASE       0xff80U
#define ZRAM_END        (ZRAM_BASE + 0x7fU)
/* Input buttons register */
#define REG_INPUT       0xff00U
/* Serial Data */
#define REG_SB          0xff01U
/* Serial Control */
#define REG_SC          0xff02U
/* Timer divider */
#define REG_DIV         0xff04U
/* Timer counter */
#define REG_TIMA        0xff05U
/* Timer modulo */
#define REG_TMA         0xff06U
/* Timer controller */
#define REG_TAC         0xff07U
/* Interrupt flags */
#define REG_IF          0xff0fU
/* Sound registers */
#define SPU_BASE        0xff10U
#define SPU_END         0xff40U
/* LCD Control register */
#define REG_LCDC        0xff40U
/* LCD Stat register */
#define REG_LCD_STAT    0xff41U
/* Background scroll Y */
#define REG_SCY         0xff42U
/* Background scroll X */
#define REG_SCX         0xff43U
/* Current line */
#define REG_LY          0xff44U
/* Current line compare */
#define REG_LYC         0xff45U
/* DMA */
#define REG_DMA         0xff46U
/* Background palette */
#define REG_BGP         0xff47U
/* Sprite palette 0 */
#define REG_OBP0        0xff48U
/* Sprite palette 1 */
#define REG_OBP1        0xff49U
/* Window Y position */
#define REG_WY          0xff4aU
/* Window X position */
#define REG_WX          0xff4bU
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

     if (addr >= IRAM_ECHO_BASE && addr < IRAM_ECHO_END) {
          return gb->iram[addr - IRAM_ECHO_BASE];
     }

     if (addr >= VRAM_BASE && addr < VRAM_END) {
          return gb->vram[addr - VRAM_BASE];
     }

     if (addr >= CRAM_BASE && addr < CRAM_END) {
          return gb_cart_ram_readb(gb, addr - CRAM_BASE);
     }

     if (addr >= SPU_BASE && addr < SPU_END) {
          /* TODO */
          return 0;
     }

     if (addr >= OAM_BASE && addr < OAM_END) {
          return gb->gpu.oam[addr - OAM_BASE];
     }

     if (addr == REG_INPUT) {
          return gb_input_get_state(gb);
     }

     if (addr == REG_SB) {
          /* XXX TODO */
          return 0xff;
     }

     if (addr == REG_SC) {
          /* XXX TODO */
          return 0;
     }

     if (addr == REG_DIV) {
          gb_timer_sync(gb);
          /* Return the high 8 bits of the divider counter */
          return gb->timer.divider_counter >> 8;
     }

     if (addr == REG_TIMA) {
          gb_timer_sync(gb);
          return gb->timer.counter;
     }

     if (addr == REG_TMA) {
          return gb->timer.modulo;
     }

     if (addr == REG_TAC) {
          return gb_timer_get_config(gb);
     }

     if (addr == REG_IF) {
          return gb->irq.irq_flags;
     }

     if (addr == REG_LCDC) {
          return gb_gpu_get_lcdc(gb);
     }

     if (addr == REG_LCD_STAT) {
          return gb_gpu_get_lcd_stat(gb);
     }

     if (addr == REG_SCY) {
          return gb->gpu.scy;
     }

     if (addr == REG_SCX) {
          return gb->gpu.scx;
     }

     if (addr == REG_LY) {
          return gb_gpu_get_ly(gb);
     }

     if (addr == REG_LYC) {
          return gb->gpu.lyc;
     }

     if (addr == REG_DMA) {
          return gb->dma.source >> 8;
     }

     if (addr == REG_BGP) {
          return gb->gpu.bgp;
     }

     if (addr == REG_OBP0) {
          return gb->gpu.obp0;
     }

     if (addr == REG_OBP1) {
          return gb->gpu.obp1;
     }

     if (addr == REG_WY) {
          return gb->gpu.wy;
     }

     if (addr == REG_WX) {
          return gb->gpu.wx;
     }

     if (addr == REG_IE) {
          return gb->irq.irq_enable;
     }

     printf("Unsupported read at address 0x%04x\n", addr);

     return 0xff;
}

void gb_memory_writeb(struct gb *gb, uint16_t addr, uint8_t val) {

     if (addr >= ROM_BASE && addr < ROM_END) {
          gb_cart_rom_writeb(gb, addr - ROM_BASE, val);
          return;
     }

     if (addr >= ZRAM_BASE && addr < ZRAM_END) {
          gb->zram[addr - ZRAM_BASE] = val;
          return;
     }

     if (addr >= IRAM_BASE && addr < IRAM_END) {
          gb->iram[addr - IRAM_BASE] = val;
          return;
     }

     if (addr >= IRAM_ECHO_BASE && addr < IRAM_ECHO_END) {
          gb->iram[addr - IRAM_ECHO_BASE] = val;
          return;
     }

     if (addr >= VRAM_BASE && addr < VRAM_END) {
          gb_gpu_sync(gb);
          gb->vram[addr - VRAM_BASE] = val;
          return;
     }

     if (addr >= CRAM_BASE && addr < CRAM_END) {
          gb_cart_ram_writeb(gb, addr - CRAM_BASE, val);
          return;
     }

     if (addr >= SPU_BASE && addr < SPU_END) {
          /* TODO */
          return;
     }

     if (addr >= OAM_BASE && addr < OAM_END) {
          gb_gpu_sync(gb);
          gb->gpu.oam[addr - OAM_BASE] = val;
          return;
     }

     if (addr == REG_INPUT) {
          gb_input_select(gb, val);
          return;
     }

     if (addr == REG_SB) {
          /* XXX TODO */
          return;
     }

     if (addr == REG_SC) {
          /* XXX TODO */
          return;
     }

     if (addr == REG_DIV) {
          gb_timer_sync(gb);
          /* Writing to the divider sets it to 0 (regardless of the value being
           * written) */
          gb->timer.divider_counter = 0;
          return;
     }

     if (addr == REG_TIMA) {
          gb_timer_sync(gb);
          gb->timer.counter = val;
          gb_timer_sync(gb);
          return;
     }

     if (addr == REG_TMA) {
          gb_timer_sync(gb);
          gb->timer.modulo = val;
          gb_timer_sync(gb);
          return;
     }

     if (addr == REG_TAC) {
          gb_timer_set_config(gb, val);
          return;
     }

     if (addr == REG_IF) {
          gb->irq.irq_flags = val | 0xE0;
          return;
     }

     if (addr == REG_IE) {
          gb->irq.irq_enable = val;
          return;
     }

     if (addr == REG_LCDC) {
          gb_gpu_set_lcdc(gb, val);
          return;
     }

     if (addr == REG_LCD_STAT) {
          gb_gpu_set_lcd_stat(gb, val);
          return;
     }

     if (addr == REG_SCY) {
          gb_gpu_sync(gb);
          gb->gpu.scy = val;
          return;
     }

     if (addr == REG_SCX) {
          gb_gpu_sync(gb);
          gb->gpu.scx = val;
          return;
     }

     if (addr == REG_LYC) {
          gb->gpu.lyc = val;
          return;
     }

     if (addr == REG_DMA) {
          gb_dma_start(gb, val);
          return;
     }

     if (addr == REG_BGP) {
          gb_gpu_sync(gb);
          gb->gpu.bgp = val;
          return;
     }

     if (addr == REG_OBP0) {
          gb_gpu_sync(gb);
          gb->gpu.obp0 = val;
          return;
     }

     if (addr == REG_OBP1) {
          gb_gpu_sync(gb);
          gb->gpu.obp1 = val;
          return;
     }

     if (addr == REG_WY) {
          gb_gpu_sync(gb);
          gb->gpu.wy = val;
          return;
     }

     if (addr == REG_WX) {
          gb_gpu_sync(gb);
          gb->gpu.wx = val;
          return;
     }

     printf("Unsupported write at address 0x%04x [val=0x%02x]\n", addr, val);
}
