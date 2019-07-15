#ifndef _GB_GB_H_
#define _GB_GB_H_

#include <stdint.h>
#include <stdlib.h>

struct gb;

#include "cpu.h"
#include "memory.h"
#include "cart.h"

struct gb {
     struct gb_cpu cpu;
     struct gb_cart cart;
};

static inline void die(void) {
     exit(EXIT_FAILURE);
}

#endif /* _GB_GB_H_ */
