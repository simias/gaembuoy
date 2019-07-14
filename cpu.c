#include <stdio.h>
#include "gb.h"

void gb_cpu_reset(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     /* XXX For the time being we don't emulate the BOOTROM so we start the
      * execution just past it */
     cpu->pc = 0x100;
}

void gb_cpu_run_instruction(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     uint8_t instruction = gb_memory_readb(gb, cpu->pc);

     cpu->pc = (cpu->pc + 1) & 0xffff;

     printf("TODO: execute 0x%02x\n", instruction);
     die();
}
