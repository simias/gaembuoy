#ifndef _GB_GB_H_
#define _GB_GB_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

struct gb;

#include "frontend.h"
#include "sync.h"
#include "irq.h"
#include "cpu.h"
#include "memory.h"
#include "cart.h"
#include "gpu.h"
#include "input.h"
#include "dma.h"
#include "timer.h"

/* DMG CPU frequency. Super GameBoy runs slightly faster (4.295454MHz). */
#define GB_CPU_FREQ_HZ 4194304U

struct gb {
     /* Counter keeping track of how many CPU cycles have elapsed since an
      * arbitrary point in time. Used to synchronize the other devices. */
     int32_t timestamp;
     /* Set by the GPU when a frame has been drawn */
     bool frame_done;
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
     struct gb_timer timer;
     /* Internal RAM */
     uint8_t iram[0x2000];
     /* Zero-page RAM */
     uint8_t zram[0x7f];
     /* Video RAM */
     uint8_t vram[0x2000];
};

static inline void die(void) {
     exit(EXIT_FAILURE);
}

#endif /* _GB_GB_H_ */
