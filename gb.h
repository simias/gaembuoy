#ifndef _GB_GB_H_
#define _GB_GB_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <semaphore.h>

struct gb;

#include "sync.h"
#include "irq.h"
#include "cpu.h"
#include "memory.h"
#include "rtc.h"
#include "cart.h"
#include "gpu.h"
#include "input.h"
#include "dma.h"
#include "hdma.h"
#include "timer.h"
#include "spu.h"
#include "frontend.h"

/* DMG CPU frequency. Super GameBoy runs slightly faster (4.295454MHz). */
#define GB_CPU_FREQ_HZ 4194304U

struct gb {
     /* True if we're emulating a GBC, false if we're emulating a DMG */
     bool gbc;

     /* True if a speed switch has been requested. It will take effect when a
      * STOP operation is executed */
     bool speed_switch_pending;
     /* True if the GBC is running in double-speed mode */
     bool double_speed;

     /* Counter keeping track of how many CPU cycles have elapsed since an
      * arbitrary point in time. Used to synchronize the other devices. */
     int32_t timestamp;
     /* Set by the frontend when the user requested that the emulation stops */
     bool quit;

     struct gb_irq irq;
     struct gb_frontend frontend;
     struct gb_sync sync;
     struct gb_cpu cpu;
     struct gb_cart cart;
     struct gb_gpu gpu;
     struct gb_input input;
     struct gb_dma dma;
     struct gb_hdma hdma;
     struct gb_timer timer;
     struct gb_spu spu;
     /* Internal RAM: 8KiB on DMG, 32 KiB on GBC */
     uint8_t iram[0x8000];
     /* Always 1 on DMG, 1-7 on GBC */
     uint8_t iram_high_bank;
     /* Zero-page RAM */
     uint8_t zram[0x7f];
     /* Video RAM: 8KiB on DMG, 16KiB on GBC */
     uint8_t vram[0x4000];
     /* Always false on DMG */
     bool    vram_high_bank;
};

static inline void die(void) {
     exit(EXIT_FAILURE);
}

#endif /* _GB_GB_H_ */
