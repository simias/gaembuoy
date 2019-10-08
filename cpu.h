#ifndef _GB_CPU_H_
#define _GB_CPU_H_

struct gb_cpu {
     /* Interrupt Master Enable (IME) flag */
     bool irq_enable;
     /* Value of IRQ enable on the next cycle (for delayed EI) */
     bool irq_enable_next;

     /* True if the CPU is currently halted */
     bool halted;

     /* Program Counter */
     uint16_t pc;
     /* Stack Pointer */
     uint16_t sp;
     /* A register */
     uint8_t a;
     /* B register */
     uint8_t b;
     /* C register */
     uint8_t c;
     /* D register */
     uint8_t d;
     /* E register */
     uint8_t e;
     /* H register */
     uint8_t h;
     /* L register */
     uint8_t l;

     /* Zero flag */
     bool f_z;
     /* Substract flag */
     bool f_n;
     /* Half-Carry flag */
     bool f_h;
     /* Carry flag */
     bool f_c;
};

void gb_cpu_reset(struct gb *gb);
int32_t gb_cpu_run_cycles(struct gb *gb, int32_t cycles);

#endif /* _GB_CPU_H_ */
