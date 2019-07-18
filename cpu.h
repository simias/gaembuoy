#ifndef _GB_CPU_H_
#define _GB_CPU_H_

struct gb_cpu {
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
void gb_cpu_run_instruction(struct gb *gb);

#endif /* _GB_CPU_H_ */
