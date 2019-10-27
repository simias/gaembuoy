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
/* Sound 1 registers */
#define REG_NR10        0xff10U
#define REG_NR11        0xff11U
#define REG_NR12        0xff12U
#define REG_NR13        0xff13U
#define REG_NR14        0xff14U
/* Sound 2 registers */
#define REG_NR21        0xff16U
#define REG_NR22        0xff17U
#define REG_NR23        0xff18U
#define REG_NR24        0xff19U
/* Sound 3 registers */
#define REG_NR30        0xff1aU
#define REG_NR31        0xff1bU
#define REG_NR32        0xff1cU
#define REG_NR33        0xff1dU
#define REG_NR34        0xff1eU
/* Sound 4 registers */
#define REG_NR41        0xff20U
#define REG_NR42        0xff21U
#define REG_NR43        0xff22U
#define REG_NR44        0xff23U
/* Sound control registers */
#define REG_NR50        0xff24U
#define REG_NR51        0xff25U
#define REG_NR52        0xff26U
/* Sound 3 waveform RAM */
#define NR3_RAM_BASE    0xff30U
#define NR3_RAM_END     0xff40U
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

/*
 * GBC-only registers
 */
/* Speed control */
#define REG_KEY1        0xff4dU
/* VRAM banking */
#define REG_VBK         0xff4fU
/* HDMA source address high */
#define REG_HDMA1       0xff51U
/* HDMA source address low */
#define REG_HDMA2       0xff52U
/* HDMA destination address high */
#define REG_HDMA3       0xff53U
/* HDMA destination address low */
#define REG_HDMA4       0xff54U
/* HDMA length, mode and start */
#define REG_HDMA5       0xff55U
/* BG palette address */
#define REG_BCPS        0xff68U
/* BG palette data */
#define REG_BCPD        0xff69U
/* Sprite palette address */
#define REG_OCPS        0xff6aU
/* Sprite palette data */
#define REG_OCPD        0xff6bU
/* Internal RAM banking */
#define REG_SVBK        0xff70U

static uint16_t gb_memory_iram_off(struct gb *gb, uint16_t off) {
     if (off >= 0x1000) {
          unsigned bank = gb->iram_high_bank;

          if (bank == 0) {
               bank = 1;
          }

          off += (bank - 1) * 0x1000;
     }

     return off;
}

/* Read one byte from memory at `addr` */
uint8_t gb_memory_readb(struct gb *gb, uint16_t addr) {
     if (addr >= ROM_BASE && addr < ROM_END) {
          return gb_cart_rom_readb(gb, addr - ROM_BASE);
     }

     if (addr >= ZRAM_BASE && addr < ZRAM_END) {
          return gb->zram[addr - ZRAM_BASE];
     }

     if (addr >= IRAM_BASE && addr < IRAM_END) {
          uint16_t off = gb_memory_iram_off(gb, addr - IRAM_BASE);

          return gb->iram[off];
     }

     if (addr >= IRAM_ECHO_BASE && addr < IRAM_ECHO_END) {
          uint16_t off = gb_memory_iram_off(gb, addr - IRAM_ECHO_BASE);

          return gb->iram[off];
     }

     if (addr >= VRAM_BASE && addr < VRAM_END) {
          uint16_t off = addr - VRAM_BASE;

          off += 0x2000 * gb->vram_high_bank;

          return gb->vram[off];
     }

     if (addr >= CRAM_BASE && addr < CRAM_END) {
          return gb_cart_ram_readb(gb, addr - CRAM_BASE);
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

     if (addr == REG_NR10) {
          uint8_t r = 0x80;

          r |= gb->spu.nr1.sweep.shift;
          r |= gb->spu.nr1.sweep.subtract << 3;
          r |= gb->spu.nr1.sweep.time << 4;

          return r;
     }

     if (addr == REG_NR11) {
          return (gb->spu.nr1.wave.duty_cycle << 6) | 0x3f;
     }

     if (addr == REG_NR12) {
          return gb->spu.nr1.envelope_config;
     }

     if (addr == REG_NR13) {
          /* Write-only */
          return 0xff;
     }

     if (addr == REG_NR14) {
          return (gb->spu.nr1.duration.enable << 6) | 0xbf;
     }

     if (addr == REG_NR21) {
          return (gb->spu.nr2.wave.duty_cycle << 6) | 0x3f;
     }

     if (addr == REG_NR22) {
          return gb->spu.nr2.envelope_config;
     }

     if (addr == REG_NR23) {
          /* Write-only */
          return 0xff;
     }

     if (addr == REG_NR24) {
          return (gb->spu.nr2.duration.enable << 6) | 0xbf;
     }

     if (addr == REG_NR30) {
          gb_spu_sync(gb);
          return (gb->spu.nr3.enable << 7) | 0x7f;
     }

     if (addr == REG_NR31) {
          return gb->spu.nr3.t1;
     }

     if (addr == REG_NR32) {
          return (gb->spu.nr3.volume_shift << 5) | 0x9f;
     }

     if (addr == REG_NR33) {
          /* Write-only */
          return 0xff;
     }

     if (addr == REG_NR34) {
          return (gb->spu.nr3.duration.enable << 6) | 0xbf;
     }

     if (addr == REG_NR41) {
          /* Read-only */
          return 0xff;
     }

     if (addr == REG_NR42) {
          return gb->spu.nr4.envelope_config;
     }

     if (addr == REG_NR43) {
          return gb->spu.nr4.lfsr_config;
     }

     if (addr == REG_NR44) {
          return (gb->spu.nr4.duration.enable << 6) | 0xbf;
     }

     if (addr == REG_NR50) {
          return gb->spu.output_level;
     }

     if (addr == REG_NR51) {
          return gb->spu.sound_mux;
     }

     if (addr == REG_NR52) {
          uint8_t r = 0;

          r |= gb->spu.nr2.running << 1;
          r |= gb->spu.nr3.running << 2;
          r |= gb->spu.enable << 7;

          return r;
     }

     if (addr >= NR3_RAM_BASE && addr < NR3_RAM_END) {
          return gb->spu.nr3.ram[addr - NR3_RAM_BASE];
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

     if (gb->gbc && addr == REG_KEY1) {
          uint8_t r = 0;

          r |= gb->double_speed << 7;
          r |= gb->speed_switch_pending;

          return r | 0x7e;
     }

     if (gb->gbc && addr == REG_VBK) {
          return gb->vram_high_bank | 0xfe;
     }

     if (gb->gbc && addr == REG_HDMA1) {
          return gb->hdma.source >> 8;
     }

     if (gb->gbc && addr == REG_HDMA2) {
          return gb->hdma.source & 0xff;
     }

     if (gb->gbc && addr == REG_HDMA3) {
          return gb->hdma.destination >> 8;
     }

     if (gb->gbc && addr == REG_HDMA4) {
          return gb->hdma.destination & 0xff;
     }

     if (gb->gbc && addr == REG_HDMA5) {
          /* The only way the CPU can read this register and see that the HDMA
           * is active is if it's configured to run on HBLANKs. If the HDMA is
           * configured to run without HBLANK it copies everything at once,
           * stopping the CPU until it's finished (and then obviously the CPU
           * can't read this register) */
          bool active = gb->hdma.run_on_hblank;
          uint8_t r = 0;

          r |= (!active) << 7;
          r |= gb->hdma.length & 0x7f;

          return r;
     }

     if (gb->gbc && addr == REG_BCPS) {
          uint8_t r = 0;

          r |= gb->gpu.bg_palettes.auto_increment << 7;
          r |= gb->gpu.bg_palettes.write_index;

          return r;
     }

     if (gb->gbc && addr == REG_BCPD) {
          struct gb_color_palette *p = &gb->gpu.bg_palettes;
          uint16_t index = p->write_index;
          unsigned palette = index >> 3;
          unsigned color_index = (index >> 1) & 3;
          bool high = index & 1;
          uint16_t col;

          col = p->colors[palette][color_index];

          if (high) {
               return col >> 8;
          } else {
               return col & 0xff;
          }
     }

     if (gb->gbc && addr == REG_OCPS) {
          uint8_t r = 0;

          r |= gb->gpu.sprite_palettes.auto_increment << 7;
          r |= gb->gpu.sprite_palettes.write_index;

          return r;
     }

     if (gb->gbc && addr == REG_OCPD) {
          struct gb_color_palette *p = &gb->gpu.sprite_palettes;
          uint16_t index = p->write_index;
          unsigned palette = index >> 3;
          unsigned color_index = (index >> 1) & 3;
          bool high = index & 1;
          uint16_t col;

          col = p->colors[palette][color_index];

          if (high) {
               return col >> 8;
          } else {
               return col & 0xff;
          }
     }

     if (gb->gbc && addr == REG_SVBK) {
          return gb->iram_high_bank | 0xf8;
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
          uint16_t off = gb_memory_iram_off(gb, addr - IRAM_BASE);

          gb->iram[off] = val;
          return;
     }

     if (addr >= IRAM_ECHO_BASE && addr < IRAM_ECHO_END) {
          uint16_t off = gb_memory_iram_off(gb, addr - IRAM_ECHO_BASE);

          gb->iram[off] = val;
          return;
     }

     if (addr >= VRAM_BASE && addr < VRAM_END) {
          uint16_t off = addr - VRAM_BASE;

          off += 0x2000 * gb->vram_high_bank;

          gb_gpu_sync(gb);
          gb->vram[off] = val;
          return;
     }

     if (addr >= CRAM_BASE && addr < CRAM_END) {
          gb_cart_ram_writeb(gb, addr - CRAM_BASE, val);
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

     if (addr == REG_NR10) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb_spu_sweep_reload(&gb->spu.nr1.sweep, val);
          }
          return;
     }

     if (addr == REG_NR11) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb->spu.nr1.wave.duty_cycle = val >> 6;
               gb_spu_duration_reload(&gb->spu.nr1.duration,
                                      GB_SPU_NR1_T1_MAX,
                                      val & 0x3f);
          }
          return;
     }

     if (addr == REG_NR12) {
          if (gb->spu.enable) {
               /* Envelope config takes effect on sound start */
               gb->spu.nr1.envelope_config = val;
          }
          return;
     }

     if (addr == REG_NR13) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb->spu.nr1.sweep.divider.offset &= 0x700;
               gb->spu.nr1.sweep.divider.offset |= val;
          }
          return;
     }

     if (addr == REG_NR14) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb->spu.nr1.sweep.divider.offset &= 0xff;
               gb->spu.nr1.sweep.divider.offset |= ((uint16_t)val & 7) << 8;

               gb->spu.nr1.duration.enable = val & 0x40;

               if (val & 0x80) {
                    gb_spu_nr1_start(gb);
               }
          }
          return;
     }

     if (addr == REG_NR21) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb->spu.nr2.wave.duty_cycle = val >> 6;
               gb_spu_duration_reload(&gb->spu.nr2.duration,
                                      GB_SPU_NR2_T1_MAX,
                                      val & 0x3f);
          }
          return;
     }

     if (addr == REG_NR22) {
          if (gb->spu.enable) {
               /* Envelope config takes effect on sound start */
               gb->spu.nr2.envelope_config = val;
          }
          return;
     }

     if (addr == REG_NR23) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb->spu.nr2.divider.offset &= 0x700;
               gb->spu.nr2.divider.offset |= val;
          }
          return;
     }

     if (addr == REG_NR24) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb->spu.nr2.divider.offset &= 0xff;
               gb->spu.nr2.divider.offset |= ((uint16_t)val & 7) << 8;

               gb->spu.nr2.duration.enable = val & 0x40;

               if (val & 0x80) {
                    gb_spu_nr2_start(gb);
               }
          }
          return;
     }

     if (addr == REG_NR30) {
          if (gb->spu.enable) {
               /* Disabling sound 3 stops it. However enabling it doesn't start
                * it until 0x80 is written in NR34. */
               bool enable = (val & 0x80);

               gb_spu_sync(gb);
               gb->spu.nr3.enable = enable;
               if (!enable) {
                    gb->spu.nr3.running = false;
               }
          }
          return;
     }

     if (addr == REG_NR31) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb->spu.nr3.t1 = val;
               gb_spu_duration_reload(&gb->spu.nr3.duration,
                                      GB_SPU_NR3_T1_MAX,
                                      val);
          }
          return;
     }

     if (addr == REG_NR32) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb->spu.nr3.volume_shift = (val >> 5) & 3;
          }
          return;
     }

     if (addr == REG_NR33) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb->spu.nr3.divider.offset &= 0x700;
               gb->spu.nr3.divider.offset |= val;
          }
          return;
     }

     if (addr == REG_NR34) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb->spu.nr3.divider.offset &= 0xff;
               gb->spu.nr3.divider.offset |= ((uint16_t)val & 7) << 8;

               gb->spu.nr3.duration.enable = val & 0x40;

               if (val & 0x80) {
                    gb_spu_nr3_start(gb);
               }
          }
          return;
     }

     if (addr == REG_NR41) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb_spu_duration_reload(&gb->spu.nr4.duration,
                                      GB_SPU_NR4_T1_MAX,
                                      val & 0x3f);
          }
          return;
     }

     if (addr == REG_NR42) {
          if (gb->spu.enable) {
               /* Envelope config takes effect on sound start */
               gb->spu.nr4.envelope_config = val;
          }
          return;
     }

     if (addr == REG_NR43) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb->spu.nr4.lfsr_config = val;
          }
          return;
     }

     if (addr == REG_NR44) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);

               gb->spu.nr4.duration.enable = val & 0x40;

               if (val & 0x80) {
                    gb_spu_nr4_start(gb);
               }
          }
          return;
     }

     if (addr == REG_NR50) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb->spu.output_level = val;
               gb_spu_update_sound_amp(gb);
          }
          return;
     }

     if (addr == REG_NR51) {
          if (gb->spu.enable) {
               gb_spu_sync(gb);
               gb->spu.sound_mux = val;
               gb_spu_update_sound_amp(gb);
          }
          return;
     }

     if (addr == REG_NR52) {
          bool enable = val & 0x80;

          if (gb->spu.enable == enable) {
               /* No change */
               return;
          }

          gb_spu_sync(gb);

          if (!enable) {
               gb_spu_reset(gb);
          }

          gb->spu.enable = enable;
          return;
     }

     if (addr >= NR3_RAM_BASE && addr < NR3_RAM_END) {
          gb->spu.nr3.ram[addr - NR3_RAM_BASE] = val;
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

     if (gb->gbc && addr == REG_KEY1) {
          gb->speed_switch_pending = val & 1;
          return;
     }

     if (gb->gbc && addr == REG_VBK) {
          gb->vram_high_bank = val & 1;
          return;
     }

     if (gb->gbc && addr == REG_HDMA1) {
          gb->hdma.source &= 0xff;
          gb->hdma.source |= (val << 8);
          return;
     }

     if (gb->gbc && addr == REG_HDMA2) {
          gb->hdma.source &= 0xff00;
          /* Low 4 bits are ignored */
          gb->hdma.source |= val & 0xf0;
          return;
     }

     if (gb->gbc && addr == REG_HDMA3) {
          gb->hdma.destination &= 0xff;
          gb->hdma.destination |= (val << 8);
          return;
     }

     if (gb->gbc && addr == REG_HDMA4) {
          gb->hdma.destination &= 0xff00;
          /* Low 4 bits are ignored (causes glitches in Oracle of Ages
           * otherwise) */
          gb->hdma.destination |= val & 0xf0;
          return;
     }

     if (gb->gbc && addr == REG_HDMA5) {
          bool run_on_hblank = val & 0x80;

          gb->hdma.length = val & 0x7f;

          if (!run_on_hblank && gb->hdma.run_on_hblank) {
               /* This stops the current transfer */
               gb_gpu_sync(gb);
               gb->hdma.run_on_hblank = false;
          } else {
               gb_hdma_start(gb, run_on_hblank);
          }

          return;
     }

     if (gb->gbc && addr == REG_BCPS) {
          gb->gpu.bg_palettes.auto_increment = val & 0x80;
          gb->gpu.bg_palettes.write_index = val & 0x3f;
          return;
     }

     if (gb->gbc && addr == REG_BCPD) {
          struct gb_color_palette *p = &gb->gpu.bg_palettes;
          uint16_t index = p->write_index;
          unsigned palette = index >> 3;
          unsigned color_index = (index >> 1) & 3;
          bool high = index & 1;
          uint16_t col;

          col = p->colors[palette][color_index];

          if (high) {
               col &= 0xff;
               col |= val << 8;
          } else {
               col &= 0xff00;
               col |= val;
          }

          p->colors[palette][color_index] = col;

          if (p->auto_increment) {
               p->write_index = (p->write_index + 1) & 0x3f;
          }

          return;
     }

     if (gb->gbc && addr == REG_OCPS) {
          gb->gpu.sprite_palettes.auto_increment = val & 0x80;
          gb->gpu.sprite_palettes.write_index = val & 0x3f;
          return;
     }

     if (gb->gbc && addr == REG_OCPD) {
          struct gb_color_palette *p = &gb->gpu.sprite_palettes;
          uint16_t index = p->write_index;
          unsigned palette = index >> 3;
          unsigned color_index = (index >> 1) & 3;
          bool high = index & 1;
          uint16_t col;

          col = p->colors[palette][color_index];

          if (high) {
               col &= 0xff;
               col |= val << 8;
          } else {
               col &= 0xff00;
               col |= val;
          }

          p->colors[palette][color_index] = col;

          if (p->auto_increment) {
               p->write_index = (p->write_index + 1) & 0x3f;
          }

          return;
     }

     if (gb->gbc && addr == REG_SVBK) {
          gb->iram_high_bank = val & 7;
          return;
     }

     printf("Unsupported write at address 0x%04x [val=0x%02x]\n", addr, val);
}
