#ifndef _GB_CPU_H_
#define _GB_CPU_H_

struct gb_cpu {
     /* Program Counter */
     uint16_t pc;
     /* Stack Pointer */
     uint16_t sp;
};

void gb_cpu_reset(struct gb *gb);
void gb_cpu_run_instruction(struct gb *gb);

#endif /* _GB_CPU_H_ */
