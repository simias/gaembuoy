#ifndef _GB_GB_H_
#define _GB_GB_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

struct gb;

#include "sync.h"
#include "cpu.h"
#include "memory.h"
#include "cart.h"
#include "gpu.h"

struct gb {
     /* Counter keeping track of how many CPU cycles have elapsed since an
      * arbitrary point in time. Used to synchronize the other devices. */
     int32_t timestamp;

     struct gb_sync sync;
     struct gb_cpu cpu;
     struct gb_cart cart;
     struct gb_gpu gpu;
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
