#include <stdio.h>
#include "gb.h"

void gb_cpu_reset(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->sp = 0xfffe;
     cpu->a  = 0;
     cpu->b  = 0;
     cpu->c  = 0;
     cpu->d  = 0;
     cpu->e  = 0;
     cpu->h  = 0;
     cpu->l  = 0;

     cpu->f_z = false;
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = false;

     /* XXX For the time being we don't emulate the BOOTROM so we start the
      * execution just past it */
     cpu->pc = 0x100;
}

static uint16_t gb_cpu_bc(struct gb *gb) {
     uint16_t b = gb->cpu.b;
     uint16_t c = gb->cpu.c;

     return (b << 8) | c;
}

static void gb_cpu_set_bc(struct gb *gb, uint16_t v) {
     gb->cpu.b = v >> 8;
     gb->cpu.c = v & 0xff;
}

static uint16_t gb_cpu_de(struct gb *gb) {
     uint16_t d = gb->cpu.d;
     uint16_t e = gb->cpu.e;

     return (d << 8) | e;
}

static void gb_cpu_set_de(struct gb *gb, uint16_t v) {
     gb->cpu.d = v >> 8;
     gb->cpu.e = v & 0xff;
}

static uint16_t gb_cpu_hl(struct gb *gb) {
     uint16_t h = gb->cpu.h;
     uint16_t l = gb->cpu.l;

     return (h << 8) | l;
}

static void gb_cpu_set_hl(struct gb *gb, uint16_t v) {
     gb->cpu.h = v >> 8;
     gb->cpu.l = v & 0xff;
}

static void gb_cpu_dump(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     fprintf(stderr, "Flags: %c %c %c %c\n",
             cpu->f_z ? 'Z' : '-',
             cpu->f_n ? 'N' : '-',
             cpu->f_h ? 'H' : '-',
             cpu->f_c ? 'C' : '-');
     fprintf(stderr, "PC: 0x%04x [%02x %02x %02x]\n",
             cpu->pc,
             gb_memory_readb(gb, cpu->pc),
             gb_memory_readb(gb, cpu->pc + 1),
             gb_memory_readb(gb, cpu->pc + 2));
     fprintf(stderr, "SP: 0x%04x [%02x %02x %02x]\n",
             cpu->sp,
             gb_memory_readb(gb, cpu->sp),
             gb_memory_readb(gb, cpu->sp + 1),
             gb_memory_readb(gb, cpu->sp + 2));
     fprintf(stderr, "A : 0x%02x\n",   cpu->a);
     fprintf(stderr, "B : 0x%02x  C : 0x%02x  BC : 0x%04x\n",
             cpu->b, cpu->c, gb_cpu_bc(gb));
     fprintf(stderr, "D : 0x%02x  E : 0x%02x  DE : 0x%04x\n",
             cpu->d, cpu->e, gb_cpu_de(gb));
     fprintf(stderr, "H : 0x%02x  L : 0x%02x  HL : 0x%04x\n",
             cpu->h, cpu->l, gb_cpu_hl(gb));
     fprintf(stderr, "\n");
}

static void gb_cpu_load_pc(struct gb *gb, uint16_t new_pc) {
     gb->cpu.pc = new_pc;
}

static void gb_cpu_pushb(struct gb *gb, uint8_t b) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->sp = (cpu->sp - 1) & 0xffff;

     gb_memory_writeb(gb, cpu->sp, b);
}

static uint8_t gb_cpu_popb(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     uint8_t b = gb_memory_readb(gb, cpu->sp);

     cpu->sp = (cpu->sp + 1) & 0xffff;

     return b;
}

static void gb_cpu_pushw(struct gb *gb, uint16_t w) {
     gb_cpu_pushb(gb, w >> 8);
     gb_cpu_pushb(gb, w & 0xff);
}

static uint16_t gb_cpu_popw(struct gb *gb) {
     uint16_t b0 = gb_cpu_popb(gb);
     uint16_t b1 = gb_cpu_popb(gb);

     return b0 | (b1 << 8);
}

static uint8_t gb_cpu_next_i8(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     uint8_t i8 = gb_memory_readb(gb, cpu->pc);

     cpu->pc = (cpu->pc + 1) & 0xffff;

     return i8;
}

static uint16_t gb_cpu_next_i16(struct gb *gb) {
     uint16_t b0 = gb_cpu_next_i8(gb);
     uint16_t b1 = gb_cpu_next_i8(gb);

     return b0 | (b1 << 8);
}

/****************
 * Instructions *
 ****************/

typedef void (*gb_instruction_f)(struct gb *);

static void gb_i_unimplemented(struct gb* gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t instruction_pc = (cpu->pc - 1) & 0xffff;
     uint8_t instruction = gb_memory_readb(gb, instruction_pc);

     fprintf(stderr,
             "Unimplemented instruction 0x%02x at 0x%04x\n",
             instruction, instruction_pc);
     die();
}

/*****************
 * Miscellaneous *
 *****************/

static void gb_i_nop(struct gb *gb) {
     // NOP
}

static void gb_i_di(struct gb *gb) {
     // XXX TODO: disable interrupts
}

/**************
 * Arithmetic *
 **************/

static uint8_t gb_cpu_inc(struct gb *gb, uint8_t v) {
     struct gb_cpu *cpu = &gb->cpu;

     uint8_t r = (v + 1) & 0xff;

     cpu->f_z = (r == 0);
     cpu->f_n = false;
     /* We'll have a half-carry if the low nibble is 0xf */
     cpu->f_h = ((v & 0xf) == 0xf);
     /* Carry is not modified by this instruction */

     return r;
}

static uint8_t gb_cpu_dec(struct gb *gb, uint8_t v) {
     struct gb_cpu *cpu = &gb->cpu;

     uint8_t r = (v - 1) & 0xff;

     cpu->f_z = (r == 0);
     cpu->f_n = true;
     /* We'll have a half-carry if the low nibble is 0 */
     cpu->f_h = ((v & 0xf) == 0);
     /* Carry is not modified by this instruction */

     return r;
}

static void gb_i_inc_a(struct gb *gb) {
     gb->cpu.a = gb_cpu_inc(gb, gb->cpu.a);
}

static void gb_i_inc_b(struct gb *gb) {
     gb->cpu.b = gb_cpu_inc(gb, gb->cpu.b);
}

static void gb_i_inc_c(struct gb *gb) {
     gb->cpu.c = gb_cpu_inc(gb, gb->cpu.c);
}

static void gb_i_inc_d(struct gb *gb) {
     gb->cpu.d = gb_cpu_inc(gb, gb->cpu.d);
}

static void gb_i_inc_e(struct gb *gb) {
     gb->cpu.e = gb_cpu_inc(gb, gb->cpu.e);
}

static void gb_i_inc_h(struct gb *gb) {
     gb->cpu.h = gb_cpu_inc(gb, gb->cpu.h);
}

static void gb_i_inc_l(struct gb *gb) {
     gb->cpu.l = gb_cpu_inc(gb, gb->cpu.l);
}

static void gb_i_dec_a(struct gb *gb) {
     gb->cpu.a = gb_cpu_dec(gb, gb->cpu.a);
}

static void gb_i_dec_b(struct gb *gb) {
     gb->cpu.b = gb_cpu_dec(gb, gb->cpu.b);
}

static void gb_i_dec_c(struct gb *gb) {
     gb->cpu.c = gb_cpu_dec(gb, gb->cpu.c);
}

static void gb_i_dec_d(struct gb *gb) {
     gb->cpu.d = gb_cpu_dec(gb, gb->cpu.d);
}

static void gb_i_dec_e(struct gb *gb) {
     gb->cpu.e = gb_cpu_dec(gb, gb->cpu.e);
}

static void gb_i_dec_h(struct gb *gb) {
     gb->cpu.h = gb_cpu_dec(gb, gb->cpu.h);
}

static void gb_i_dec_l(struct gb *gb) {
     gb->cpu.l = gb_cpu_dec(gb, gb->cpu.l);
}

/* Add two 16 bit values, update the CPU flags and return the result */
static uint16_t gb_cpu_addw_set_flags(struct gb *gb, uint16_t a, uint16_t b) {
     struct gb_cpu *cpu = &gb->cpu;

     /* Widen to 32bits to get the carry */
     uint32_t wa = a;
     uint32_t wb = b;

     uint32_t r = a + b;

     cpu->f_n = false;
     cpu->f_c = r & 0x10000;
     cpu->f_h = (wa ^ wb ^ r) & 0x1000;
     /* cpu->f_z is not altered */

     return r;
}

static uint8_t gb_cpu_sub_set_flags(struct gb *gb, uint8_t a, uint8_t b) {
     struct gb_cpu *cpu = &gb->cpu;

     /* Check for carry using 16bit arithmetic */
     uint16_t al = a;
     uint16_t bl = b;

     uint16_t r = al - bl;

     cpu->f_z = !(r & 0xff);
     cpu->f_n = true;
     cpu->f_h = (a ^ b ^ r) & 0x10;
     cpu->f_c = r & 0x100;

     return r;
}

static void gb_i_sub_a_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = 0;

     cpu->f_z = true;
     cpu->f_n = true;
     cpu->f_h = false;
     cpu->f_c = false;
}

static uint8_t gb_cpu_add_set_flags(struct gb *gb, uint8_t a, uint8_t b) {
     struct gb_cpu *cpu = &gb->cpu;

     /* Check for carry using 16bit arithmetic */
     uint16_t al = a;
     uint16_t bl = b;

     uint16_t r = al + bl;

     cpu->f_z = !(r & 0xff);
     cpu->f_n = false;
     cpu->f_h = (a ^ b ^ r) & 0x10;
     cpu->f_c = r & 0x100;

     return r;
}

static void gb_i_add_a_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_add_set_flags(gb, cpu->a, cpu->a);
}

static void gb_i_add_a_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_add_set_flags(gb, cpu->a, cpu->b);
}

static void gb_i_add_a_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_add_set_flags(gb, cpu->a, cpu->c);
}

static void gb_i_add_a_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_add_set_flags(gb, cpu->a, cpu->d);
}

static void gb_i_add_a_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_add_set_flags(gb, cpu->a, cpu->e);
}

static void gb_i_add_a_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_add_set_flags(gb, cpu->a, cpu->h);
}

static void gb_i_add_a_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_add_set_flags(gb, cpu->a, cpu->l);
}

static void gb_i_add_sp_si8(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     /* Offset is signed */
     int8_t i8 = gb_cpu_next_i8(gb);

     /* Use 32bit arithmetic to avoid signed integer overflow UB */
     int32_t r = cpu->sp;
     r += i8;

     cpu->f_z = false;
     cpu->f_n = false;
     /* Carry and Half-carry are for the low byte */
     cpu->f_h = (cpu->sp ^ i8 ^ r) & 0x10;
     cpu->f_c = (cpu->sp ^ i8 ^ r) & 0x100;

     cpu->sp = r;
}

static void gb_i_add_hl_bc(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint16_t bc = gb_cpu_bc(gb);

     hl = gb_cpu_addw_set_flags(gb, hl, bc);

     gb_cpu_set_hl(gb, hl);
}

static void gb_i_add_hl_de(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint16_t de = gb_cpu_de(gb);

     hl = gb_cpu_addw_set_flags(gb, hl, de);

     gb_cpu_set_hl(gb, hl);
}

static void gb_i_add_hl_hl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     hl = gb_cpu_addw_set_flags(gb, hl, hl);

     gb_cpu_set_hl(gb, hl);
}

static void gb_i_add_hl_sp(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     hl = gb_cpu_addw_set_flags(gb, hl, gb->cpu.sp);

     gb_cpu_set_hl(gb, hl);
}

static void gb_i_inc_sp(struct gb *gb) {
     uint16_t sp = gb->cpu.sp;

     sp = (sp + 1) & 0xffff;

     gb->cpu.sp = sp;
}

static void gb_i_inc_bc(struct gb *gb) {
     uint16_t bc = gb_cpu_bc(gb);

     bc = (bc + 1) & 0xffff;

     gb_cpu_set_bc(gb, bc);
}

static void gb_i_inc_de(struct gb *gb) {
     uint16_t de = gb_cpu_de(gb);

     de = (de + 1) & 0xffff;

     gb_cpu_set_de(gb, de);
}

static void gb_i_inc_hl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     hl = (hl + 1) & 0xffff;

     gb_cpu_set_hl(gb, hl);
}

static void gb_i_cp_a_i8(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);

     gb_cpu_sub_set_flags(gb, gb->cpu.a, i8);
}

/*********
 * Loads *
 *********/

static void gb_i_ld_a_i8(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);

     gb->cpu.a = i8;
}

static void gb_i_ld_b_i8(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);

     gb->cpu.b = i8;
}

static void gb_i_ld_c_i8(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);

     gb->cpu.c = i8;
}

static void gb_i_ld_d_i8(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);

     gb->cpu.d = i8;
}

static void gb_i_ld_e_i8(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);

     gb->cpu.e = i8;
}

static void gb_i_ld_h_i8(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);

     gb->cpu.h = i8;
}

static void gb_i_ld_l_i8(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);

     gb->cpu.l = i8;
}

static void gb_i_ld_mi16_a(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb_memory_writeb(gb, i16, gb->cpu.a);
}

static void gb_i_ldh_mi8_a(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);
     uint16_t addr = 0xff00 | i8;

     gb_memory_writeb(gb, addr, gb->cpu.a);
}

static void gb_i_ldh_a_mi8(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);
     uint16_t addr = 0xff00 | i8;

     gb->cpu.a = gb_memory_readb(gb, addr);
}

static void gb_i_ld_bc_i16(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb_cpu_set_bc(gb, i16);
}

static void gb_i_ld_de_i16(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb_cpu_set_de(gb, i16);
}

static void gb_i_ld_sp_i16(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb->cpu.sp = i16;
}

static void gb_i_ld_hl_i16(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb_cpu_set_hl(gb, i16);
}

static void gb_i_ld_mbc_a(struct gb *gb) {
     uint16_t bc = gb_cpu_bc(gb);
     uint16_t a = gb->cpu.a;

     gb_memory_writeb(gb, bc, a);
}

static void gb_i_ld_mde_a(struct gb *gb) {
     uint16_t de = gb_cpu_de(gb);
     uint16_t a = gb->cpu.a;

     gb_memory_writeb(gb, de, a);
}

static void gb_i_ld_mhl_a(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint16_t a = gb->cpu.a;

     gb_memory_writeb(gb, hl, a);
}

static void gb_i_ldi_mhl_a(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint16_t a = gb->cpu.a;

     gb_memory_writeb(gb, hl, a);

     hl = (hl + 1) & 0xffff;
     gb_cpu_set_hl(gb, hl);
}

static void gb_i_ldd_mhl_a(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint16_t a = gb->cpu.a;

     gb_memory_writeb(gb, hl, a);

     hl = (hl - 1) & 0xffff;
     gb_cpu_set_hl(gb, hl);
}

static void gb_i_ld_a_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_memory_readb(gb, hl);

     gb->cpu.a = v;
}

static void gb_i_ldi_a_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     gb->cpu.a = gb_memory_readb(gb, hl);

     hl = (hl + 1) & 0xffff;
     gb_cpu_set_hl(gb, hl);
}

static void gb_i_ldd_a_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     gb->cpu.a = gb_memory_readb(gb, hl);

     hl = (hl - 1) & 0xffff;
     gb_cpu_set_hl(gb, hl);
}

static void gb_i_ld_a_mbc(struct gb *gb) {
     uint16_t bc = gb_cpu_bc(gb);
     uint8_t v = gb_memory_readb(gb, bc);

     gb->cpu.a = v;
}

static void gb_i_ld_a_mde(struct gb *gb) {
     uint16_t de = gb_cpu_de(gb);
     uint8_t v = gb_memory_readb(gb, de);

     gb->cpu.a = v;
}

static void gb_i_ld_b_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_memory_readb(gb, hl);

     gb->cpu.b = v;
}

static void gb_i_ld_c_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_memory_readb(gb, hl);

     gb->cpu.c = v;
}

static void gb_i_ld_d_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_memory_readb(gb, hl);

     gb->cpu.d = v;
}

static void gb_i_ld_e_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_memory_readb(gb, hl);

     gb->cpu.e = v;
}

static void gb_i_ld_h_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_memory_readb(gb, hl);

     gb->cpu.h = v;
}

static void gb_i_ld_l_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_memory_readb(gb, hl);

     gb->cpu.l = v;
}

static void gb_i_push_hl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     gb_cpu_pushw(gb, hl);
}

static void gb_i_pop_bc(struct gb *gb) {
     uint16_t bc = gb_cpu_popw(gb);

     gb_cpu_set_bc(gb, bc);
}

static void gb_i_pop_de(struct gb *gb) {
     uint16_t de = gb_cpu_popw(gb);

     gb_cpu_set_de(gb, de);
}

static void gb_i_pop_hl(struct gb *gb) {
     uint16_t hl = gb_cpu_popw(gb);

     gb_cpu_set_hl(gb, hl);
}

static void gb_i_pop_af(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t f = gb_cpu_popb(gb);
     uint8_t a = gb_cpu_popb(gb);

     cpu->a = a;

     /* Restore flags from memory (low 4 bits are ignored) */
     cpu->f_z = f & (1U << 7);
     cpu->f_n = f & (1U << 6);
     cpu->f_h = f & (1U << 5);
     cpu->f_c = f & (1U << 4);
}

static void gb_i_ld_a_b(struct gb *gb) {
     gb->cpu.a = gb->cpu.b;
}

static void gb_i_ld_a_c(struct gb *gb) {
     gb->cpu.a = gb->cpu.c;
}

static void gb_i_ld_a_d(struct gb *gb) {
     gb->cpu.a = gb->cpu.d;
}

static void gb_i_ld_a_e(struct gb *gb) {
     gb->cpu.a = gb->cpu.e;
}

static void gb_i_ld_a_h(struct gb *gb) {
     gb->cpu.a = gb->cpu.h;
}

static void gb_i_ld_a_l(struct gb *gb) {
     gb->cpu.a = gb->cpu.l;
}

static void gb_i_ld_b_a(struct gb *gb) {
     gb->cpu.b = gb->cpu.a;
}

static void gb_i_ld_b_c(struct gb *gb) {
     gb->cpu.b = gb->cpu.c;
}

static void gb_i_ld_b_d(struct gb *gb) {
     gb->cpu.b = gb->cpu.d;
}

static void gb_i_ld_b_e(struct gb *gb) {
     gb->cpu.b = gb->cpu.e;
}

static void gb_i_ld_b_h(struct gb *gb) {
     gb->cpu.b = gb->cpu.h;
}

static void gb_i_ld_b_l(struct gb *gb) {
     gb->cpu.b = gb->cpu.l;
}

static void gb_i_ld_c_a(struct gb *gb) {
     gb->cpu.c = gb->cpu.a;
}

static void gb_i_ld_c_b(struct gb *gb) {
     gb->cpu.c = gb->cpu.b;
}

static void gb_i_ld_c_d(struct gb *gb) {
     gb->cpu.c = gb->cpu.d;
}

static void gb_i_ld_c_e(struct gb *gb) {
     gb->cpu.c = gb->cpu.e;
}

static void gb_i_ld_c_h(struct gb *gb) {
     gb->cpu.c = gb->cpu.h;
}

static void gb_i_ld_c_l(struct gb *gb) {
     gb->cpu.c = gb->cpu.l;
}

static void gb_i_ld_d_a(struct gb *gb) {
     gb->cpu.d = gb->cpu.a;
}

static void gb_i_ld_d_b(struct gb *gb) {
     gb->cpu.d = gb->cpu.b;
}

static void gb_i_ld_d_c(struct gb *gb) {
     gb->cpu.d = gb->cpu.c;
}

static void gb_i_ld_d_e(struct gb *gb) {
     gb->cpu.d = gb->cpu.e;
}

static void gb_i_ld_d_h(struct gb *gb) {
     gb->cpu.d = gb->cpu.h;
}

static void gb_i_ld_d_l(struct gb *gb) {
     gb->cpu.d = gb->cpu.l;
}

static void gb_i_ld_e_a(struct gb *gb) {
     gb->cpu.e = gb->cpu.a;
}

static void gb_i_ld_e_b(struct gb *gb) {
     gb->cpu.e = gb->cpu.b;
}

static void gb_i_ld_e_c(struct gb *gb) {
     gb->cpu.e = gb->cpu.c;
}

static void gb_i_ld_e_d(struct gb *gb) {
     gb->cpu.e = gb->cpu.d;
}

static void gb_i_ld_e_h(struct gb *gb) {
     gb->cpu.e = gb->cpu.h;
}

static void gb_i_ld_e_l(struct gb *gb) {
     gb->cpu.e = gb->cpu.l;
}

static void gb_i_ld_h_a(struct gb *gb) {
     gb->cpu.h = gb->cpu.a;
}

static void gb_i_ld_h_b(struct gb *gb) {
     gb->cpu.h = gb->cpu.b;
}

static void gb_i_ld_h_c(struct gb *gb) {
     gb->cpu.h = gb->cpu.c;
}

static void gb_i_ld_h_d(struct gb *gb) {
     gb->cpu.h = gb->cpu.d;
}

static void gb_i_ld_h_e(struct gb *gb) {
     gb->cpu.h = gb->cpu.e;
}

static void gb_i_ld_h_l(struct gb *gb) {
     gb->cpu.h = gb->cpu.l;
}

static void gb_i_ld_l_a(struct gb *gb) {
     gb->cpu.l = gb->cpu.a;
}

static void gb_i_ld_l_b(struct gb *gb) {
     gb->cpu.l = gb->cpu.b;
}

static void gb_i_ld_l_c(struct gb *gb) {
     gb->cpu.l = gb->cpu.c;
}

static void gb_i_ld_l_d(struct gb *gb) {
     gb->cpu.l = gb->cpu.d;
}

static void gb_i_ld_l_e(struct gb *gb) {
     gb->cpu.l = gb->cpu.e;
}

static void gb_i_ld_l_h(struct gb *gb) {
     gb->cpu.l = gb->cpu.h;
}

/***************
 * Jumps/Calls *
 ***************/

static void gb_i_jp_i16(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb_cpu_load_pc(gb, i16);
}

static void gb_i_jr_si8(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t i8 = gb_cpu_next_i8(gb);
     uint16_t pc = cpu->pc;

     pc = pc + (int8_t)i8;

     gb_cpu_load_pc(gb, pc);
}

static void gb_i_jr_z_si8(struct gb *gb) {
     if (gb->cpu.f_z) {
          gb_i_jr_si8(gb);
     } else {
          /* Discard immediate value */
          gb_cpu_next_i8(gb);
     }
}

static void gb_i_jr_c_si8(struct gb *gb) {
     if (gb->cpu.f_c) {
          gb_i_jr_si8(gb);
     } else {
          /* Discard immediate value */
          gb_cpu_next_i8(gb);
     }
}

static void gb_i_jr_nz_si8(struct gb *gb) {
     if (!gb->cpu.f_z) {
          gb_i_jr_si8(gb);
     } else {
          /* Discard immediate value */
          gb_cpu_next_i8(gb);
     }
}

static void gb_i_jr_nc_si8(struct gb *gb) {
     if (!gb->cpu.f_c) {
          gb_i_jr_si8(gb);
     } else {
          /* Discard immediate value */
          gb_cpu_next_i8(gb);
     }
}

static void gb_i_call_i16(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb_cpu_pushw(gb, gb->cpu.pc);

     gb_cpu_load_pc(gb, i16);
}

static void gb_i_ret(struct gb *gb) {
     uint16_t addr = gb_cpu_popw(gb);

     gb_cpu_load_pc(gb, addr);
}

static void gb_i_ret_z(struct gb *gb) {
     if (gb->cpu.f_z) {
          gb_i_ret(gb);
     }
}

static void gb_i_ret_c(struct gb *gb) {
     if (gb->cpu.f_c) {
          gb_i_ret(gb);
     }
}

static void gb_i_ret_nz(struct gb *gb) {
     if (!gb->cpu.f_z) {
          gb_i_ret(gb);
     }
}

static void gb_i_ret_nc(struct gb *gb) {
     if (!gb->cpu.f_c) {
          gb_i_ret(gb);
     }
}

static gb_instruction_f gb_instructions[0x100] = {
     // 0x00
     gb_i_nop,
     gb_i_ld_bc_i16,
     gb_i_ld_mbc_a,
     gb_i_inc_bc,
     gb_i_inc_b,
     gb_i_dec_b,
     gb_i_ld_b_i8,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_add_hl_bc,
     gb_i_ld_a_mbc,
     gb_i_unimplemented,
     gb_i_inc_c,
     gb_i_dec_c,
     gb_i_ld_c_i8,
     gb_i_unimplemented,
     // 0x10
     gb_i_unimplemented,
     gb_i_ld_de_i16,
     gb_i_ld_mde_a,
     gb_i_inc_de,
     gb_i_inc_d,
     gb_i_dec_d,
     gb_i_ld_d_i8,
     gb_i_unimplemented,
     gb_i_jr_si8,
     gb_i_add_hl_de,
     gb_i_ld_a_mde,
     gb_i_unimplemented,
     gb_i_inc_e,
     gb_i_dec_e,
     gb_i_ld_e_i8,
     gb_i_unimplemented,
     // 0x20
     gb_i_jr_nz_si8,
     gb_i_ld_hl_i16,
     gb_i_ldi_mhl_a,
     gb_i_inc_hl,
     gb_i_inc_h,
     gb_i_dec_h,
     gb_i_ld_h_i8,
     gb_i_unimplemented,
     gb_i_jr_z_si8,
     gb_i_add_hl_hl,
     gb_i_ldi_a_mhl,
     gb_i_unimplemented,
     gb_i_inc_l,
     gb_i_dec_l,
     gb_i_ld_l_i8,
     gb_i_unimplemented,
     // 0x30
     gb_i_jr_nc_si8,
     gb_i_ld_sp_i16,
     gb_i_ldd_mhl_a,
     gb_i_inc_sp,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_jr_c_si8,
     gb_i_add_hl_sp,
     gb_i_ldd_a_mhl,
     gb_i_unimplemented,
     gb_i_inc_a,
     gb_i_dec_a,
     gb_i_ld_a_i8,
     gb_i_unimplemented,
     // 0x40
     gb_i_nop,
     gb_i_ld_b_c,
     gb_i_ld_b_d,
     gb_i_ld_b_e,
     gb_i_ld_b_h,
     gb_i_ld_b_l,
     gb_i_ld_b_mhl,
     gb_i_ld_b_a,
     gb_i_ld_c_b,
     gb_i_nop,
     gb_i_ld_c_d,
     gb_i_ld_c_e,
     gb_i_ld_c_h,
     gb_i_ld_c_l,
     gb_i_ld_c_mhl,
     gb_i_ld_c_a,
     // 0x50
     gb_i_ld_d_b,
     gb_i_ld_d_c,
     gb_i_nop,
     gb_i_ld_d_e,
     gb_i_ld_d_h,
     gb_i_ld_d_l,
     gb_i_ld_d_mhl,
     gb_i_ld_d_a,
     gb_i_ld_e_b,
     gb_i_ld_e_c,
     gb_i_ld_e_d,
     gb_i_nop,
     gb_i_ld_e_h,
     gb_i_ld_e_l,
     gb_i_ld_e_mhl,
     gb_i_ld_e_a,
     // 0x60
     gb_i_ld_h_b,
     gb_i_ld_h_c,
     gb_i_ld_h_d,
     gb_i_ld_h_e,
     gb_i_nop,
     gb_i_ld_h_l,
     gb_i_ld_h_mhl,
     gb_i_ld_h_a,
     gb_i_ld_l_b,
     gb_i_ld_l_c,
     gb_i_ld_l_d,
     gb_i_ld_l_e,
     gb_i_ld_l_h,
     gb_i_nop,
     gb_i_ld_l_mhl,
     gb_i_ld_l_a,
     // 0x70
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_ld_mhl_a,
     gb_i_ld_a_b,
     gb_i_ld_a_c,
     gb_i_ld_a_d,
     gb_i_ld_a_e,
     gb_i_ld_a_h,
     gb_i_ld_a_l,
     gb_i_ld_a_mhl,
     gb_i_nop,
     // 0x80
     gb_i_add_a_b,
     gb_i_add_a_c,
     gb_i_add_a_d,
     gb_i_add_a_e,
     gb_i_add_a_h,
     gb_i_add_a_l,
     gb_i_unimplemented,
     gb_i_add_a_a,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0x90
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_sub_a_a,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0xa0
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0xb0
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0xc0
     gb_i_ret_nz,
     gb_i_pop_bc,
     gb_i_unimplemented,
     gb_i_jp_i16,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_ret_z,
     gb_i_ret,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_call_i16,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0xd0
     gb_i_ret_nc,
     gb_i_pop_de,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_ret_c,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0xe0
     gb_i_ldh_mi8_a,
     gb_i_pop_hl,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_push_hl,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_add_sp_si8,
     gb_i_unimplemented,
     gb_i_ld_mi16_a,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     // 0xf0
     gb_i_ldh_a_mi8,
     gb_i_pop_af,
     gb_i_unimplemented,
     gb_i_di,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_unimplemented,
     gb_i_cp_a_i8,
     gb_i_unimplemented,
};

void gb_cpu_run_instruction(struct gb *gb) {
     uint8_t instruction;

     gb_cpu_dump(gb);

     instruction = gb_cpu_next_i8(gb);

     gb_instructions[instruction](gb);
}
