#include <stdio.h>
#include <assert.h>
#include "gb.h"

void gb_cpu_reset(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->irq_enable = false;
     cpu->irq_enable_next = false;

     cpu->halted = false;

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

     if (gb->gbc) {
          /* In GBC mode the boot ROM sets A to 0x11 before starting the game.
           * The game can use this to detect whether it's running on DMG or GBC.
           */
          cpu->a = 0x11;
     }

}

static inline void gb_cpu_clock_tick(struct gb *gb, int32_t cycles) {
     gb->timestamp += cycles >> gb->double_speed;

     if (gb->timestamp >= gb->sync.first_event) {
          /* We have a device sync pending */
          gb_sync_check_events(gb);
     }
}

static uint8_t gb_cpu_readb(struct gb *gb, uint16_t addr) {
     uint8_t b = gb_memory_readb(gb, addr);

     gb_cpu_clock_tick(gb, 4);

     return b;
}

static void gb_cpu_writeb(struct gb *gb, uint16_t addr, uint8_t val) {
     gb_memory_writeb(gb, addr, val);

     gb_cpu_clock_tick(gb, 4);
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

void gb_cpu_dump(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     fprintf(stderr, "Flags: %c %c %c %c  IME: %d\n",
             cpu->f_z ? 'Z' : '-',
             cpu->f_n ? 'N' : '-',
             cpu->f_h ? 'H' : '-',
             cpu->f_c ? 'C' : '-',
             cpu->irq_enable);
     fprintf(stderr, "PC: 0x%04x [%02x %02x %02x]\n",
             cpu->pc,
             gb_memory_readb(gb, cpu->pc),
             gb_memory_readb(gb, cpu->pc + 1),
             gb_memory_readb(gb, cpu->pc + 2));
     fprintf(stderr, "SP: 0x%04x\n", cpu->sp);
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

     gb_cpu_clock_tick(gb, 4);
}

static void gb_cpu_pushb(struct gb *gb, uint8_t b) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->sp = (cpu->sp - 1) & 0xffff;

     gb_cpu_writeb(gb, cpu->sp, b);
}

static uint8_t gb_cpu_popb(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     uint8_t b = gb_cpu_readb(gb, cpu->sp);

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

     uint8_t i8 = gb_cpu_readb(gb, cpu->pc);

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

/*****************
 * Miscellaneous *
 *****************/

static void gb_i_nop(struct gb *gb) {
     // NOP
}

static void gb_i_undefined(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t instruction_pc = (cpu->pc - 1) & 0xffff;
     uint8_t instruction = gb_memory_readb(gb, instruction_pc);

     // Undefined opcode. Apparently freezes the CPU on real hardware
     fprintf(stderr,
             "Undefined instruction instruction 0x%02x at 0x%04x\n",
             instruction, instruction_pc);
     die();
}

static void gb_i_di(struct gb *gb) {
     gb->cpu.irq_enable = false;
     gb->cpu.irq_enable_next = false;
}

static void gb_i_ei(struct gb *gb) {
     /* Interrupts are re-enabled after the *next* instruction */
     gb->cpu.irq_enable_next = true;
}

static void gb_i_stop(struct gb *gb) {
     if (gb->speed_switch_pending) {
          /* If a speed change has been requested it is executed on STOP and the
           * execution resumes normally after that. */
          /* XXX: this should take about 16ms when switching to double-speed
           * mode and about 32ms when switching back to normal mode */

          /* Clock speed is going to change, synchronize the relevant devices
           * with the current clock speed */
          gb_timer_sync(gb);
          gb_dma_sync(gb);

          gb->double_speed = !gb->double_speed;

          /* Re-sync with new prediction */
          gb_timer_sync(gb);
          gb_dma_sync(gb);

          return;
     }

     // XXX TODO: stop CPU and screen until button press
     fprintf(stderr, "Implement STOP!\n");
     die();
}

/* Halt and wait for interrupt */
static void gb_i_halt(struct gb *gb) {
     gb->cpu.halted = true;
}

/* Set Carry Flag */
static void gb_i_scf(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = true;
}

/* Complement Carry Flag */
static void gb_i_ccf(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = !cpu->f_c;
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

static void gb_i_inc_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_cpu_readb(gb, hl);

     v = gb_cpu_inc(gb, v);

     gb_cpu_writeb(gb, hl, v);
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

static void gb_i_dec_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_cpu_readb(gb, hl);

     v = gb_cpu_dec(gb, v);

     gb_cpu_writeb(gb, hl, v);
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

     gb_cpu_clock_tick(gb, 4);

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

     cpu->a = gb_cpu_sub_set_flags(gb, cpu->a, cpu->a);
}

static void gb_i_sub_a_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_sub_set_flags(gb, cpu->a, cpu->b);
}

static void gb_i_sub_a_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_sub_set_flags(gb, cpu->a, cpu->c);
}

static void gb_i_sub_a_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_sub_set_flags(gb, cpu->a, cpu->d);
}

static void gb_i_sub_a_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_sub_set_flags(gb, cpu->a, cpu->e);
}

static void gb_i_sub_a_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_sub_set_flags(gb, cpu->a, cpu->h);
}

static void gb_i_sub_a_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_sub_set_flags(gb, cpu->a, cpu->l);
}

static void gb_i_sub_a_mhl(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);
     cpu->a = gb_cpu_sub_set_flags(gb, cpu->a, v);
}

static void gb_i_sub_a_i8(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t i8 = gb_cpu_next_i8(gb);

     cpu->a = gb_cpu_sub_set_flags(gb, cpu->a, i8);
}

/* Subtract with carry */
static uint8_t gb_cpu_sbc_set_flags(struct gb *gb, uint8_t a, uint8_t b) {
     struct gb_cpu *cpu = &gb->cpu;

     /* Check for carry using 16bit arithmetic */
     uint16_t al = a;
     uint16_t bl = b;
     uint16_t c = cpu->f_c;

     uint16_t r = al - bl - c;

     cpu->f_z = !(r & 0xff);
     cpu->f_n = true;
     cpu->f_h = (a ^ b ^ r) & 0x10;
     cpu->f_c = r & 0x100;

     return r;
}

static void gb_i_sbc_a_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_sbc_set_flags(gb, cpu->a, cpu->a);
}

static void gb_i_sbc_a_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_sbc_set_flags(gb, cpu->a, cpu->b);
}

static void gb_i_sbc_a_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_sbc_set_flags(gb, cpu->a, cpu->c);
}

static void gb_i_sbc_a_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_sbc_set_flags(gb, cpu->a, cpu->d);
}

static void gb_i_sbc_a_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_sbc_set_flags(gb, cpu->a, cpu->e);
}

static void gb_i_sbc_a_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_sbc_set_flags(gb, cpu->a, cpu->h);
}

static void gb_i_sbc_a_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_sbc_set_flags(gb, cpu->a, cpu->l);
}

static void gb_i_sbc_a_mhl(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);
     cpu->a = gb_cpu_sbc_set_flags(gb, cpu->a, v);
}

static void gb_i_sbc_a_i8(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t i8 = gb_cpu_next_i8(gb);

     cpu->a = gb_cpu_sbc_set_flags(gb, cpu->a, i8);
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

static void gb_i_add_a_mhl(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);
     cpu->a = gb_cpu_add_set_flags(gb, cpu->a, v);
}

static void gb_i_add_a_i8(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t i8 = gb_cpu_next_i8(gb);

     cpu->a = gb_cpu_add_set_flags(gb, cpu->a, i8);
}

/* Add with carry and set flags */
static uint8_t gb_cpu_adc_set_flags(struct gb *gb, uint8_t a, uint8_t b) {
     struct gb_cpu *cpu = &gb->cpu;

     /* Check for carry using 16bit arithmetic */
     uint16_t al = a;
     uint16_t bl = b;
     uint16_t c = cpu->f_c;

     uint16_t r = al + bl + c;

     cpu->f_z = !(r & 0xff);
     cpu->f_n = false;
     cpu->f_h = (a ^ b ^ r) & 0x10;
     cpu->f_c = r & 0x100;

     return r;
}

static void gb_i_adc_a_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_adc_set_flags(gb, cpu->a, cpu->a);
}

static void gb_i_adc_a_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_adc_set_flags(gb, cpu->a, cpu->b);
}

static void gb_i_adc_a_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_adc_set_flags(gb, cpu->a, cpu->c);
}

static void gb_i_adc_a_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_adc_set_flags(gb, cpu->a, cpu->d);
}

static void gb_i_adc_a_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_adc_set_flags(gb, cpu->a, cpu->e);
}

static void gb_i_adc_a_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_adc_set_flags(gb, cpu->a, cpu->h);
}

static void gb_i_adc_a_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_adc_set_flags(gb, cpu->a, cpu->l);
}

static void gb_i_adc_a_mhl(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);
     cpu->a = gb_cpu_adc_set_flags(gb, cpu->a, v);
}

static void gb_i_adc_a_i8(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t i8 = gb_cpu_next_i8(gb);

     cpu->a = gb_cpu_adc_set_flags(gb, cpu->a, i8);
}

static uint16_t gb_add_sp_si8(struct gb *gb) {
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

     return (uint16_t)r;
}

static void gb_i_add_sp_si8(struct gb *gb) {
     gb->cpu.sp = gb_add_sp_si8(gb);

     gb_cpu_clock_tick(gb, 8);
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

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_inc_bc(struct gb *gb) {
     uint16_t bc = gb_cpu_bc(gb);

     bc = (bc + 1) & 0xffff;

     gb_cpu_set_bc(gb, bc);

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_inc_de(struct gb *gb) {
     uint16_t de = gb_cpu_de(gb);

     de = (de + 1) & 0xffff;

     gb_cpu_set_de(gb, de);

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_inc_hl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     hl = (hl + 1) & 0xffff;

     gb_cpu_set_hl(gb, hl);

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_dec_sp(struct gb *gb) {
     uint16_t sp = gb->cpu.sp;

     sp = (sp - 1) & 0xffff;

     gb->cpu.sp = sp;

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_dec_bc(struct gb *gb) {
     uint16_t bc = gb_cpu_bc(gb);

     bc = (bc - 1) & 0xffff;

     gb_cpu_set_bc(gb, bc);

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_dec_de(struct gb *gb) {
     uint16_t de = gb_cpu_de(gb);

     de = (de - 1) & 0xffff;

     gb_cpu_set_de(gb, de);

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_dec_hl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     hl = (hl - 1) & 0xffff;

     gb_cpu_set_hl(gb, hl);

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_cp_a_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sub_set_flags(gb, cpu->a, cpu->a);
}

static void gb_i_cp_a_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sub_set_flags(gb, cpu->a, cpu->b);
}

static void gb_i_cp_a_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sub_set_flags(gb, cpu->a, cpu->c);
}

static void gb_i_cp_a_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sub_set_flags(gb, cpu->a, cpu->d);
}

static void gb_i_cp_a_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sub_set_flags(gb, cpu->a, cpu->e);
}

static void gb_i_cp_a_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sub_set_flags(gb, cpu->a, cpu->h);
}

static void gb_i_cp_a_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sub_set_flags(gb, cpu->a, cpu->l);
}

static void gb_i_cp_a_mhl(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);
     gb_cpu_sub_set_flags(gb, cpu->a, v);
}

static void gb_i_cp_a_i8(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t i8 = gb_cpu_next_i8(gb);

     gb_cpu_sub_set_flags(gb, cpu->a, i8);
}

static uint8_t gb_cpu_and_set_flags(struct gb *gb, uint8_t a, uint8_t b) {
     struct gb_cpu *cpu = &gb->cpu;

     uint8_t r = a & b;

     cpu->f_z = (r == 0);
     cpu->f_n = false;
     cpu->f_h = true;
     cpu->f_c = false;

     return r;
}

static void gb_i_and_a_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_and_set_flags(gb, cpu->a, cpu->a);
}

static void gb_i_and_a_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_and_set_flags(gb, cpu->a, cpu->b);
}

static void gb_i_and_a_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_and_set_flags(gb, cpu->a, cpu->c);
}

static void gb_i_and_a_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_and_set_flags(gb, cpu->a, cpu->d);
}

static void gb_i_and_a_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_and_set_flags(gb, cpu->a, cpu->e);
}

static void gb_i_and_a_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_and_set_flags(gb, cpu->a, cpu->h);
}

static void gb_i_and_a_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_and_set_flags(gb, cpu->a, cpu->l);
}

static void gb_i_and_a_mhl(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);
     cpu->a = gb_cpu_and_set_flags(gb, cpu->a, v);
}

static void gb_i_and_a_i8(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t i8 = gb_cpu_next_i8(gb);

     cpu->a = gb_cpu_and_set_flags(gb, cpu->a, i8);
}

static uint8_t gb_cpu_xor_set_flags(struct gb *gb, uint8_t a, uint8_t b) {
     struct gb_cpu *cpu = &gb->cpu;

     uint8_t r = a ^ b;

     cpu->f_z = (r == 0);
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = false;

     return r;
}

static void gb_i_xor_a_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_xor_set_flags(gb, cpu->a, cpu->a);
}

static void gb_i_xor_a_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_xor_set_flags(gb, cpu->a, cpu->b);
}

static void gb_i_xor_a_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_xor_set_flags(gb, cpu->a, cpu->c);
}

static void gb_i_xor_a_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_xor_set_flags(gb, cpu->a, cpu->d);
}

static void gb_i_xor_a_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_xor_set_flags(gb, cpu->a, cpu->e);
}

static void gb_i_xor_a_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_xor_set_flags(gb, cpu->a, cpu->h);
}

static void gb_i_xor_a_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_xor_set_flags(gb, cpu->a, cpu->l);
}

static void gb_i_xor_a_mhl(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);
     cpu->a = gb_cpu_xor_set_flags(gb, cpu->a, v);
}

static void gb_i_xor_a_i8(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t i8 = gb_cpu_next_i8(gb);

     cpu->a = gb_cpu_xor_set_flags(gb, cpu->a, i8);
}

static uint8_t gb_cpu_or_set_flags(struct gb *gb, uint8_t a, uint8_t b) {
     struct gb_cpu *cpu = &gb->cpu;

     uint8_t r = a | b;

     cpu->f_z = (r == 0);
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = false;

     return r;
}

static void gb_i_or_a_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_or_set_flags(gb, cpu->a, cpu->a);
}

static void gb_i_or_a_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_or_set_flags(gb, cpu->a, cpu->b);
}

static void gb_i_or_a_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_or_set_flags(gb, cpu->a, cpu->c);
}

static void gb_i_or_a_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_or_set_flags(gb, cpu->a, cpu->d);
}

static void gb_i_or_a_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_or_set_flags(gb, cpu->a, cpu->e);
}

static void gb_i_or_a_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_or_set_flags(gb, cpu->a, cpu->h);
}

static void gb_i_or_a_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     cpu->a = gb_cpu_or_set_flags(gb, cpu->a, cpu->l);
}

static void gb_i_or_a_mhl(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);
     cpu->a = gb_cpu_or_set_flags(gb, cpu->a, v);
}

static void gb_i_or_a_i8(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t i8 = gb_cpu_next_i8(gb);

     cpu->a = gb_cpu_or_set_flags(gb, cpu->a, i8);
}

static void gb_i_cpl_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     /* Complement A */
     cpu->a = ~cpu->a;

     cpu->f_n = true;
     cpu->f_h = true;
}

/* Rotate Left A */
static void gb_i_rlca(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t a = cpu->a;
     uint8_t c;

     c = a >> 7;
     a = (a << 1) & 0xff;
     a |= c;

     cpu->a = a;

     cpu->f_z = false;
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = c;
}

/* Rotate Left A through carry */
static void gb_i_rla(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t a = cpu->a;
     uint8_t c = cpu->f_c;
     uint8_t new_c;

     /* Current carry goes to LSB of A, MSB of A becomes new carry */
     new_c = a >> 7;
     a = (a << 1) & 0xff;
     a |= c;

     cpu->a = a;

     cpu->f_z = false;
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = new_c;
}

/* Rotate Right A */
static void gb_i_rrca(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t a = cpu->a;
     uint8_t c;

     c = a & 1;
     a = a >> 1;
     a |= (c << 7);

     cpu->a = a;

     cpu->f_z = false;
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = c;
}

/* Rotate Right A through carry */
static void gb_i_rra(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t a = cpu->a;
     uint8_t c = cpu->f_c;
     uint8_t new_c;

     /* Current carry goes to MSB of A, LSB of A becomes new carry */
     new_c = a & 1;
     a = a >> 1;
     a |= (c << 7);

     cpu->a = a;

     cpu->f_z = false;
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = new_c;
}

/* Decimal adjust `A` for BCD operations */
static void gb_i_daa(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t a = cpu->a;
     uint8_t adj = 0;

     /* See if we had a carry/borrow for the low nibble in the last operation */
     if (cpu->f_h) {
          /* Yes, we have to adjust it. */
          adj |= 0x06;
     }

     /* See if we had a carry/borrow for the high nibble in the last operation */
     if (cpu->f_c) {
          // Yes, we have to adjust it.
          adj |= 0x60;
     }

     if (cpu->f_n) {
          /* If the operation was a substraction we're done since we can never
           * end up in the A-F range by substracting without generating a
           * (half)carry. */
          a -= adj;
     } else {
          /* Additions are a bit more tricky because we might have to adjust
           * even if we haven't overflowed (and no carry is present). For
           * instance: 0x8 + 0x4 -> 0xc. */
          if ((a & 0xf) > 0x09) {
               adj |= 0x06;
          }

          if (a > 0x99) {
               adj |= 0x60;
          }

          a += adj;
        };

     cpu->a = a;
     cpu->f_z = (a == 0);
     cpu->f_c = ((adj & 0x60) != 0);
     cpu->f_h = false;
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

static void gb_i_ld_mhl_i8(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);
     uint16_t hl = gb_cpu_hl(gb);

     gb_cpu_writeb(gb, hl, i8);
}

static void gb_i_ld_mi16_a(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb_cpu_writeb(gb, i16, gb->cpu.a);
}

static void gb_i_ld_mi16_sp(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);
     uint16_t sp = gb->cpu.sp;

     gb_cpu_writeb(gb, i16, sp & 0xff);
     gb_cpu_writeb(gb, i16 + 1, sp >> 8);
}

static void gb_i_ld_a_mi16(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb->cpu.a = gb_cpu_readb(gb, i16);
}

static void gb_i_ldh_mi8_a(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);
     uint16_t addr = 0xff00 | i8;

     gb_cpu_writeb(gb, addr, gb->cpu.a);
}

static void gb_i_ldh_a_mi8(struct gb *gb) {
     uint8_t i8 = gb_cpu_next_i8(gb);
     uint16_t addr = 0xff00 | i8;

     gb->cpu.a = gb_cpu_readb(gb, addr);
}

static void gb_i_ldh_mc_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t addr = 0xff00 | cpu->c;

     gb_cpu_writeb(gb, addr, cpu->a);
}

static void gb_i_ldh_a_mc(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t addr = 0xff00 | cpu->c;

     cpu->a = gb_cpu_readb(gb, addr);
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

static void gb_i_ld_sp_hl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     gb->cpu.sp = hl;

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_ld_hl_i16(struct gb *gb) {
     uint16_t i16 = gb_cpu_next_i16(gb);

     gb_cpu_set_hl(gb, i16);
}

static void gb_i_ld_mbc_a(struct gb *gb) {
     uint16_t bc = gb_cpu_bc(gb);
     uint16_t a = gb->cpu.a;

     gb_cpu_writeb(gb, bc, a);
}

static void gb_i_ld_mde_a(struct gb *gb) {
     uint16_t de = gb_cpu_de(gb);
     uint16_t a = gb->cpu.a;

     gb_cpu_writeb(gb, de, a);
}

static void gb_i_ld_mhl_a(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint16_t a = gb->cpu.a;

     gb_cpu_writeb(gb, hl, a);
}

static void gb_i_ld_mhl_b(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint16_t b = gb->cpu.b;

     gb_cpu_writeb(gb, hl, b);
}

static void gb_i_ld_mhl_c(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint16_t c = gb->cpu.c;

     gb_cpu_writeb(gb, hl, c);
}

static void gb_i_ld_mhl_d(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint16_t d = gb->cpu.d;

     gb_cpu_writeb(gb, hl, d);
}

static void gb_i_ld_mhl_e(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint16_t e = gb->cpu.e;

     gb_cpu_writeb(gb, hl, e);
}

static void gb_i_ld_mhl_h(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint16_t h = gb->cpu.h;

     gb_cpu_writeb(gb, hl, h);
}

static void gb_i_ld_mhl_l(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint16_t l = gb->cpu.l;

     gb_cpu_writeb(gb, hl, l);
}

static void gb_i_ldi_mhl_a(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint16_t a = gb->cpu.a;

     gb_cpu_writeb(gb, hl, a);

     hl = (hl + 1) & 0xffff;
     gb_cpu_set_hl(gb, hl);
}

static void gb_i_ldd_mhl_a(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint16_t a = gb->cpu.a;

     gb_cpu_writeb(gb, hl, a);

     hl = (hl - 1) & 0xffff;
     gb_cpu_set_hl(gb, hl);
}

static void gb_i_ld_a_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_cpu_readb(gb, hl);

     gb->cpu.a = v;
}

static void gb_i_ldi_a_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     gb->cpu.a = gb_cpu_readb(gb, hl);

     hl = (hl + 1) & 0xffff;
     gb_cpu_set_hl(gb, hl);
}

static void gb_i_ldd_a_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     gb->cpu.a = gb_cpu_readb(gb, hl);

     hl = (hl - 1) & 0xffff;
     gb_cpu_set_hl(gb, hl);
}

static void gb_i_ld_a_mbc(struct gb *gb) {
     uint16_t bc = gb_cpu_bc(gb);
     uint8_t v = gb_cpu_readb(gb, bc);

     gb->cpu.a = v;
}

static void gb_i_ld_a_mde(struct gb *gb) {
     uint16_t de = gb_cpu_de(gb);
     uint8_t v = gb_cpu_readb(gb, de);

     gb->cpu.a = v;
}

static void gb_i_ld_b_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_cpu_readb(gb, hl);

     gb->cpu.b = v;
}

static void gb_i_ld_c_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_cpu_readb(gb, hl);

     gb->cpu.c = v;
}

static void gb_i_ld_d_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_cpu_readb(gb, hl);

     gb->cpu.d = v;
}

static void gb_i_ld_e_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_cpu_readb(gb, hl);

     gb->cpu.e = v;
}

static void gb_i_ld_h_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_cpu_readb(gb, hl);

     gb->cpu.h = v;
}

static void gb_i_ld_l_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v = gb_cpu_readb(gb, hl);

     gb->cpu.l = v;
}

static void gb_i_ld_hl_sp_si8(struct gb *gb) {
     uint16_t hl = gb_add_sp_si8(gb);

     gb_cpu_set_hl(gb, hl);

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_push_bc(struct gb *gb) {
     uint16_t bc = gb_cpu_bc(gb);

     gb_cpu_pushw(gb, bc);

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_push_de(struct gb *gb) {
     uint16_t de = gb_cpu_de(gb);

     gb_cpu_pushw(gb, de);

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_push_hl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     gb_cpu_pushw(gb, hl);

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_push_af(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t f = 0;

     f |= cpu->f_z << 7;
     f |= cpu->f_n << 6;
     f |= cpu->f_h << 5;
     f |= cpu->f_c << 4;

     gb_cpu_pushb(gb, cpu->a);
     gb_cpu_pushb(gb, f);

     gb_cpu_clock_tick(gb, 4);
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

static void gb_i_jp_nz_i16(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t i16 = gb_cpu_next_i16(gb);

     if (!cpu->f_z) {
          gb_cpu_load_pc(gb, i16);
     }
}

static void gb_i_jp_z_i16(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t i16 = gb_cpu_next_i16(gb);

     if (cpu->f_z) {
          gb_cpu_load_pc(gb, i16);
     }
}

static void gb_i_jp_nc_i16(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t i16 = gb_cpu_next_i16(gb);

     if (!cpu->f_c) {
          gb_cpu_load_pc(gb, i16);
     }
}

static void gb_i_jp_c_i16(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     uint16_t i16 = gb_cpu_next_i16(gb);

     if (cpu->f_c) {
          gb_cpu_load_pc(gb, i16);
     }
}

static void gb_i_jp_hl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);

     /* This doesn't incur any additional delay so we don't call gb_cpu_load_pc
      */
     gb->cpu.pc = hl;
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

static void gb_i_call_nz_i16(struct gb *gb) {
     if (!gb->cpu.f_z) {
          gb_i_call_i16(gb);
     } else {
          /* Discard immediate value */
          gb_cpu_next_i16(gb);
     }
}

static void gb_i_call_z_i16(struct gb *gb) {
     if (gb->cpu.f_z) {
          gb_i_call_i16(gb);
     } else {
          /* Discard immediate value */
          gb_cpu_next_i16(gb);
     }
}

static void gb_i_call_nc_i16(struct gb *gb) {
     if (!gb->cpu.f_c) {
          gb_i_call_i16(gb);
     } else {
          /* Discard immediate value */
          gb_cpu_next_i16(gb);
     }
}

static void gb_i_call_c_i16(struct gb *gb) {
     if (gb->cpu.f_c) {
          gb_i_call_i16(gb);
     } else {
          /* Discard immediate value */
          gb_cpu_next_i16(gb);
     }
}

static void gb_i_ret(struct gb *gb) {
     uint16_t addr = gb_cpu_popw(gb);

     gb_cpu_load_pc(gb, addr);
}

static void gb_i_ret_z(struct gb *gb) {
     if (gb->cpu.f_z) {
          gb_i_ret(gb);
     }

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_ret_c(struct gb *gb) {
     if (gb->cpu.f_c) {
          gb_i_ret(gb);
     }

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_ret_nz(struct gb *gb) {
     if (!gb->cpu.f_z) {
          gb_i_ret(gb);
     }

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_ret_nc(struct gb *gb) {
     if (!gb->cpu.f_c) {
          gb_i_ret(gb);
     }

     gb_cpu_clock_tick(gb, 4);
}

static void gb_i_reti(struct gb *gb) {
     gb_i_ret(gb);

     gb->cpu.irq_enable = true;
     gb->cpu.irq_enable_next = true;
}

static void gb_cpu_rst(struct gb *gb, uint16_t target) {
     gb_cpu_pushw(gb, gb->cpu.pc);

     gb_cpu_load_pc(gb, target);
}

static void gb_i_rst_00(struct gb *gb) {
     gb_cpu_rst(gb, 0x00);
}

static void gb_i_rst_08(struct gb *gb) {
     gb_cpu_rst(gb, 0x08);
}

static void gb_i_rst_10(struct gb *gb) {
     gb_cpu_rst(gb, 0x10);
}

static void gb_i_rst_18(struct gb *gb) {
     gb_cpu_rst(gb, 0x18);
}

static void gb_i_rst_20(struct gb *gb) {
     gb_cpu_rst(gb, 0x20);
}

static void gb_i_rst_28(struct gb *gb) {
     gb_cpu_rst(gb, 0x28);
}

static void gb_i_rst_30(struct gb *gb) {
     gb_cpu_rst(gb, 0x30);
}

static void gb_i_rst_38(struct gb *gb) {
     gb_cpu_rst(gb, 0x38);
}

static void gb_i_op_cb(struct gb *gb);

static gb_instruction_f gb_instructions[0x100] = {
     // 0x00
     gb_i_nop,
     gb_i_ld_bc_i16,
     gb_i_ld_mbc_a,
     gb_i_inc_bc,
     gb_i_inc_b,
     gb_i_dec_b,
     gb_i_ld_b_i8,
     gb_i_rlca,
     gb_i_ld_mi16_sp,
     gb_i_add_hl_bc,
     gb_i_ld_a_mbc,
     gb_i_dec_bc,
     gb_i_inc_c,
     gb_i_dec_c,
     gb_i_ld_c_i8,
     gb_i_rrca,
     // 0x10
     gb_i_stop,
     gb_i_ld_de_i16,
     gb_i_ld_mde_a,
     gb_i_inc_de,
     gb_i_inc_d,
     gb_i_dec_d,
     gb_i_ld_d_i8,
     gb_i_rla,
     gb_i_jr_si8,
     gb_i_add_hl_de,
     gb_i_ld_a_mde,
     gb_i_dec_de,
     gb_i_inc_e,
     gb_i_dec_e,
     gb_i_ld_e_i8,
     gb_i_rra,
     // 0x20
     gb_i_jr_nz_si8,
     gb_i_ld_hl_i16,
     gb_i_ldi_mhl_a,
     gb_i_inc_hl,
     gb_i_inc_h,
     gb_i_dec_h,
     gb_i_ld_h_i8,
     gb_i_daa,
     gb_i_jr_z_si8,
     gb_i_add_hl_hl,
     gb_i_ldi_a_mhl,
     gb_i_dec_hl,
     gb_i_inc_l,
     gb_i_dec_l,
     gb_i_ld_l_i8,
     gb_i_cpl_a,
     // 0x30
     gb_i_jr_nc_si8,
     gb_i_ld_sp_i16,
     gb_i_ldd_mhl_a,
     gb_i_inc_sp,
     gb_i_inc_mhl,
     gb_i_dec_mhl,
     gb_i_ld_mhl_i8,
     gb_i_scf,
     gb_i_jr_c_si8,
     gb_i_add_hl_sp,
     gb_i_ldd_a_mhl,
     gb_i_dec_sp,
     gb_i_inc_a,
     gb_i_dec_a,
     gb_i_ld_a_i8,
     gb_i_ccf,
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
     gb_i_ld_mhl_b,
     gb_i_ld_mhl_c,
     gb_i_ld_mhl_d,
     gb_i_ld_mhl_e,
     gb_i_ld_mhl_h,
     gb_i_ld_mhl_l,
     gb_i_halt,
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
     gb_i_add_a_mhl,
     gb_i_add_a_a,
     gb_i_adc_a_b,
     gb_i_adc_a_c,
     gb_i_adc_a_d,
     gb_i_adc_a_e,
     gb_i_adc_a_h,
     gb_i_adc_a_l,
     gb_i_adc_a_mhl,
     gb_i_adc_a_a,
     // 0x90
     gb_i_sub_a_b,
     gb_i_sub_a_c,
     gb_i_sub_a_d,
     gb_i_sub_a_e,
     gb_i_sub_a_h,
     gb_i_sub_a_l,
     gb_i_sub_a_mhl,
     gb_i_sub_a_a,
     gb_i_sbc_a_b,
     gb_i_sbc_a_c,
     gb_i_sbc_a_d,
     gb_i_sbc_a_e,
     gb_i_sbc_a_h,
     gb_i_sbc_a_l,
     gb_i_sbc_a_mhl,
     gb_i_sbc_a_a,
     // 0xa0
     gb_i_and_a_b,
     gb_i_and_a_c,
     gb_i_and_a_d,
     gb_i_and_a_e,
     gb_i_and_a_h,
     gb_i_and_a_l,
     gb_i_and_a_mhl,
     gb_i_and_a_a,
     gb_i_xor_a_b,
     gb_i_xor_a_c,
     gb_i_xor_a_d,
     gb_i_xor_a_e,
     gb_i_xor_a_h,
     gb_i_xor_a_l,
     gb_i_xor_a_mhl,
     gb_i_xor_a_a,
     // 0xb0
     gb_i_or_a_b,
     gb_i_or_a_c,
     gb_i_or_a_d,
     gb_i_or_a_e,
     gb_i_or_a_h,
     gb_i_or_a_l,
     gb_i_or_a_mhl,
     gb_i_or_a_a,
     gb_i_cp_a_b,
     gb_i_cp_a_c,
     gb_i_cp_a_d,
     gb_i_cp_a_e,
     gb_i_cp_a_h,
     gb_i_cp_a_l,
     gb_i_cp_a_mhl,
     gb_i_cp_a_a,
     // 0xc0
     gb_i_ret_nz,
     gb_i_pop_bc,
     gb_i_jp_nz_i16,
     gb_i_jp_i16,
     gb_i_call_nz_i16,
     gb_i_push_bc,
     gb_i_add_a_i8,
     gb_i_rst_00,
     gb_i_ret_z,
     gb_i_ret,
     gb_i_jp_z_i16,
     gb_i_op_cb,
     gb_i_call_z_i16,
     gb_i_call_i16,
     gb_i_adc_a_i8,
     gb_i_rst_08,
     // 0xd0
     gb_i_ret_nc,
     gb_i_pop_de,
     gb_i_jp_nc_i16,
     gb_i_undefined,
     gb_i_call_nc_i16,
     gb_i_push_de,
     gb_i_sub_a_i8,
     gb_i_rst_10,
     gb_i_ret_c,
     gb_i_reti,
     gb_i_jp_c_i16,
     gb_i_undefined,
     gb_i_call_c_i16,
     gb_i_undefined,
     gb_i_sbc_a_i8,
     gb_i_rst_18,
     // 0xe0
     gb_i_ldh_mi8_a,
     gb_i_pop_hl,
     gb_i_ldh_mc_a,
     gb_i_undefined,
     gb_i_undefined,
     gb_i_push_hl,
     gb_i_and_a_i8,
     gb_i_rst_20,
     gb_i_add_sp_si8,
     gb_i_jp_hl,
     gb_i_ld_mi16_a,
     gb_i_undefined,
     gb_i_undefined,
     gb_i_undefined,
     gb_i_xor_a_i8,
     gb_i_rst_28,
     // 0xf0
     gb_i_ldh_a_mi8,
     gb_i_pop_af,
     gb_i_ldh_a_mc,
     gb_i_di,
     gb_i_undefined,
     gb_i_push_af,
     gb_i_or_a_i8,
     gb_i_rst_30,
     gb_i_ld_hl_sp_si8,
     gb_i_ld_sp_hl,
     gb_i_ld_a_mi16,
     gb_i_ei,
     gb_i_undefined,
     gb_i_undefined,
     gb_i_cp_a_i8,
     gb_i_rst_38,
};

/* Addresses of the interrupt handlers in memory */
static const uint16_t gb_irq_handlers[5] = {
     [GB_IRQ_VSYNC]    = 0x0040,
     [GB_IRQ_LCD_STAT] = 0x0048,
     [GB_IRQ_TIMER]    = 0x0050,
     [GB_IRQ_SERIAL]   = 0x0058,
     [GB_IRQ_INPUT]    = 0x0060
};

static void gb_cpu_check_interrupts(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;
     struct gb_irq *irq = &gb->irq;
     uint8_t active_irq;
     uint16_t handler;
     unsigned i;

     /* See if we have an interrupt pending */
     active_irq = irq->irq_enable & irq->irq_flags & 0x1f;

     if (!active_irq) {
          return;
     }

     /* We have an active IRQ, that gets us outside of halted mode even if the
      * IME is not set in the CPU */
     cpu->halted = false;

     if (!cpu->irq_enable) {
          /* IME is not set, nothing to do */
          return;
     }

     /* Find the first active IRQ. The order is significant, IRQs with a lower
      * number have the priority. */
     for (i = 0; i < 5; i++) {
          if (active_irq & (1U << i)) {
               break;
          }
     }

     /* That shouldn't happen since we check if we have an active IRQ above */
     assert(i < 5);

     handler = gb_irq_handlers[i];

     cpu->irq_enable = false;
     cpu->irq_enable_next = false;

     /* Entering Interrupt context takes 12 cycles */
     gb_cpu_clock_tick(gb, 12);

     /* Push current PC on the stack */
     gb_cpu_pushw(gb, gb->cpu.pc);

     /* We're about to handle this interrupt, acknowledge it */
     irq->irq_flags &= ~(1U << i);

     /* Jump to the IRQ handler */
     gb_cpu_load_pc(gb, handler);
}

static void gb_cpu_run_instruction(struct gb *gb) {
     uint8_t instruction;

     instruction = gb_cpu_next_i8(gb);

     gb_instructions[instruction](gb);
}

int32_t gb_cpu_run_cycles(struct gb *gb, int32_t cycles) {
     struct gb_cpu *cpu = &gb->cpu;
     /* Rebase the synchronization timestamps, which has the side effect of
      * setting gb->timestamp to 0 */
     gb_sync_rebase(gb);

     while (gb->timestamp < cycles) {
          /* We check for interrupt before anything else since it could get us
           * out of halted mode */
          gb_cpu_check_interrupts(gb);
          cpu->irq_enable = cpu->irq_enable_next;

          if (cpu->halted) {
               int32_t skip_cycles;

               /* The CPU is halted so we skip to the next event or `cycles`,
                * whichever comes first */
               if (cycles < gb->sync.first_event) {
                    skip_cycles = cycles - gb->timestamp;
               } else {
                    skip_cycles = gb->sync.first_event - gb->timestamp;
               }

               gb_cpu_clock_tick(gb, skip_cycles << gb->double_speed);

               /* See if any event needs to run. This may trigger an IRQ which
                * will un-halt the CPU in the next iteration */
               gb_sync_check_events(gb);

          } else {
               gb_cpu_run_instruction(gb);
          }
     }

     return gb->timestamp;
}

/*
 * Extended CB instruction map
 */

static void gb_cpu_rlc_set_flags(struct gb *gb, uint8_t *v) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t c = *v >> 7;

     *v = (*v << 1) | c;

     cpu->f_z = (*v == 0);
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = c;
}

static void gb_i_rlc_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rlc_set_flags(gb, &cpu->a);
}

static void gb_i_rlc_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rlc_set_flags(gb, &cpu->b);
}

static void gb_i_rlc_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rlc_set_flags(gb, &cpu->c);
}

static void gb_i_rlc_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rlc_set_flags(gb, &cpu->d);
}

static void gb_i_rlc_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rlc_set_flags(gb, &cpu->e);
}

static void gb_i_rlc_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rlc_set_flags(gb, &cpu->h);
}

static void gb_i_rlc_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rlc_set_flags(gb, &cpu->l);
}

static void gb_i_rlc_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_rlc_set_flags(gb, &v);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_cpu_rrc_set_flags(struct gb *gb, uint8_t *v) {
     struct gb_cpu *cpu = &gb->cpu;
     uint8_t c = *v & 1;

     *v = (*v >> 1) | (c << 7);

     cpu->f_z = (*v == 0);
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = c;
}

static void gb_i_rrc_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rrc_set_flags(gb, &cpu->a);
}

static void gb_i_rrc_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rrc_set_flags(gb, &cpu->b);
}

static void gb_i_rrc_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rrc_set_flags(gb, &cpu->c);
}

static void gb_i_rrc_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rrc_set_flags(gb, &cpu->d);
}

static void gb_i_rrc_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rrc_set_flags(gb, &cpu->e);
}

static void gb_i_rrc_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rrc_set_flags(gb, &cpu->h);
}

static void gb_i_rrc_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rrc_set_flags(gb, &cpu->l);
}

static void gb_i_rrc_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_rrc_set_flags(gb, &v);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_cpu_rl_set_flags(struct gb *gb, uint8_t *v) {
     struct gb_cpu *cpu = &gb->cpu;
     bool new_c = *v >> 7;

     *v = (*v << 1) | (uint8_t)cpu->f_c;

     cpu->f_z = (*v == 0);
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = new_c;
}

static void gb_i_rl_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rl_set_flags(gb, &cpu->a);
}

static void gb_i_rl_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rl_set_flags(gb, &cpu->b);
}

static void gb_i_rl_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rl_set_flags(gb, &cpu->c);
}

static void gb_i_rl_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rl_set_flags(gb, &cpu->d);
}

static void gb_i_rl_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rl_set_flags(gb, &cpu->e);
}

static void gb_i_rl_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rl_set_flags(gb, &cpu->h);
}

static void gb_i_rl_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rl_set_flags(gb, &cpu->l);
}

static void gb_i_rl_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_rl_set_flags(gb, &v);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_cpu_rr_set_flags(struct gb *gb, uint8_t *v) {
     struct gb_cpu *cpu = &gb->cpu;
     bool new_c = *v & 1;
     uint8_t old_c = cpu->f_c;

     *v = (*v >> 1) | (old_c << 7);

     cpu->f_z = (*v == 0);
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = new_c;
}

static void gb_i_rr_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rr_set_flags(gb, &cpu->a);
}

static void gb_i_rr_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rr_set_flags(gb, &cpu->b);
}

static void gb_i_rr_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rr_set_flags(gb, &cpu->c);
}

static void gb_i_rr_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rr_set_flags(gb, &cpu->d);
}

static void gb_i_rr_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rr_set_flags(gb, &cpu->e);
}

static void gb_i_rr_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rr_set_flags(gb, &cpu->h);
}

static void gb_i_rr_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_rr_set_flags(gb, &cpu->l);
}

static void gb_i_rr_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_rr_set_flags(gb, &v);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_cpu_sla_set_flags(struct gb *gb, uint8_t *v) {
     struct gb_cpu *cpu = &gb->cpu;
     bool c = *v >> 7;

     *v = *v << 1;

     cpu->f_z = (*v == 0);
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = c;
}

static void gb_i_sla_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sla_set_flags(gb, &cpu->a);
}

static void gb_i_sla_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sla_set_flags(gb, &cpu->b);
}

static void gb_i_sla_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sla_set_flags(gb, &cpu->c);
}

static void gb_i_sla_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sla_set_flags(gb, &cpu->d);
}

static void gb_i_sla_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sla_set_flags(gb, &cpu->e);
}

static void gb_i_sla_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sla_set_flags(gb, &cpu->h);
}

static void gb_i_sla_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sla_set_flags(gb, &cpu->l);
}

static void gb_i_sla_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_sla_set_flags(gb, &v);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_cpu_sra_set_flags(struct gb *gb, uint8_t *v) {
     struct gb_cpu *cpu = &gb->cpu;
     bool c = *v & 1;

     /* Sign-extend */
     *v = (*v >> 1) | (*v & 0x80);

     cpu->f_z = (*v == 0);
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = c;
}

static void gb_i_sra_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sra_set_flags(gb, &cpu->a);
}

static void gb_i_sra_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sra_set_flags(gb, &cpu->b);
}

static void gb_i_sra_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sra_set_flags(gb, &cpu->c);
}

static void gb_i_sra_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sra_set_flags(gb, &cpu->d);
}

static void gb_i_sra_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sra_set_flags(gb, &cpu->e);
}

static void gb_i_sra_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sra_set_flags(gb, &cpu->h);
}

static void gb_i_sra_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_sra_set_flags(gb, &cpu->l);
}

static void gb_i_sra_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_sra_set_flags(gb, &v);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_cpu_swap_set_flags(struct gb *gb, uint8_t *v) {
     struct gb_cpu *cpu = &gb->cpu;

     *v = ((*v << 4) | (*v >> 4)) & 0xff;

     cpu->f_z = (*v == 0);
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = false;
}

static void gb_i_swap_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_swap_set_flags(gb, &cpu->a);
}

static void gb_i_swap_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_swap_set_flags(gb, &cpu->b);
}

static void gb_i_swap_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_swap_set_flags(gb, &cpu->c);
}

static void gb_i_swap_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_swap_set_flags(gb, &cpu->d);
}

static void gb_i_swap_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_swap_set_flags(gb, &cpu->e);
}

static void gb_i_swap_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_swap_set_flags(gb, &cpu->h);
}

static void gb_i_swap_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_swap_set_flags(gb, &cpu->l);
}

static void gb_i_swap_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_swap_set_flags(gb, &v);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_cpu_srl_set_flags(struct gb *gb, uint8_t *v) {
     struct gb_cpu *cpu = &gb->cpu;
     bool c = *v & 1;

     *v = *v >> 1;

     cpu->f_z = (*v == 0);
     cpu->f_n = false;
     cpu->f_h = false;
     cpu->f_c = c;
}

static void gb_i_srl_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_srl_set_flags(gb, &cpu->a);
}

static void gb_i_srl_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_srl_set_flags(gb, &cpu->b);
}

static void gb_i_srl_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_srl_set_flags(gb, &cpu->c);
}

static void gb_i_srl_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_srl_set_flags(gb, &cpu->d);
}

static void gb_i_srl_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_srl_set_flags(gb, &cpu->e);
}

static void gb_i_srl_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_srl_set_flags(gb, &cpu->h);
}

static void gb_i_srl_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_srl_set_flags(gb, &cpu->l);
}

static void gb_i_srl_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_srl_set_flags(gb, &v);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_cpu_bit_set_flags(struct gb *gb, uint8_t *v, unsigned bit) {
     struct gb_cpu *cpu = &gb->cpu;
     bool set = *v & (1U << bit);

     cpu->f_z = !set;
     cpu->f_n = false;
     cpu->f_h = true;
}

static void gb_i_bit_0_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->a, 0);
}

static void gb_i_bit_0_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->b, 0);
}

static void gb_i_bit_0_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->c, 0);
}

static void gb_i_bit_0_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->d, 0);
}

static void gb_i_bit_0_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->e, 0);
}

static void gb_i_bit_0_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->h, 0);
}

static void gb_i_bit_0_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->l, 0);
}

static void gb_i_bit_0_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_bit_set_flags(gb, &v, 0);
}

static void gb_i_bit_1_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->a, 1);
}

static void gb_i_bit_1_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->b, 1);
}

static void gb_i_bit_1_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->c, 1);
}

static void gb_i_bit_1_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->d, 1);
}

static void gb_i_bit_1_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->e, 1);
}

static void gb_i_bit_1_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->h, 1);
}

static void gb_i_bit_1_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->l, 1);
}

static void gb_i_bit_1_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_bit_set_flags(gb, &v, 1);
}

static void gb_i_bit_2_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->a, 2);
}

static void gb_i_bit_2_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->b, 2);
}

static void gb_i_bit_2_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->c, 2);
}

static void gb_i_bit_2_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->d, 2);
}

static void gb_i_bit_2_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->e, 2);
}

static void gb_i_bit_2_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->h, 2);
}

static void gb_i_bit_2_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->l, 2);
}

static void gb_i_bit_2_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_bit_set_flags(gb, &v, 2);
}

static void gb_i_bit_3_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->a, 3);
}

static void gb_i_bit_3_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->b, 3);
}

static void gb_i_bit_3_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->c, 3);
}

static void gb_i_bit_3_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->d, 3);
}

static void gb_i_bit_3_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->e, 3);
}

static void gb_i_bit_3_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->h, 3);
}

static void gb_i_bit_3_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->l, 3);
}

static void gb_i_bit_3_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_bit_set_flags(gb, &v, 3);
}

static void gb_i_bit_4_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->a, 4);
}

static void gb_i_bit_4_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->b, 4);
}

static void gb_i_bit_4_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->c, 4);
}

static void gb_i_bit_4_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->d, 4);
}

static void gb_i_bit_4_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->e, 4);
}

static void gb_i_bit_4_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->h, 4);
}

static void gb_i_bit_4_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->l, 4);
}

static void gb_i_bit_4_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_bit_set_flags(gb, &v, 4);
}

static void gb_i_bit_5_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->a, 5);
}

static void gb_i_bit_5_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->b, 5);
}

static void gb_i_bit_5_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->c, 5);
}

static void gb_i_bit_5_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->d, 5);
}

static void gb_i_bit_5_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->e, 5);
}

static void gb_i_bit_5_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->h, 5);
}

static void gb_i_bit_5_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->l, 5);
}

static void gb_i_bit_5_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_bit_set_flags(gb, &v, 5);
}

static void gb_i_bit_6_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->a, 6);
}

static void gb_i_bit_6_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->b, 6);
}

static void gb_i_bit_6_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->c, 6);
}

static void gb_i_bit_6_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->d, 6);
}

static void gb_i_bit_6_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->e, 6);
}

static void gb_i_bit_6_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->h, 6);
}

static void gb_i_bit_6_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->l, 6);
}

static void gb_i_bit_6_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_bit_set_flags(gb, &v, 6);
}

static void gb_i_bit_7_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->a, 7);
}

static void gb_i_bit_7_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->b, 7);
}

static void gb_i_bit_7_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->c, 7);
}

static void gb_i_bit_7_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->d, 7);
}

static void gb_i_bit_7_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->e, 7);
}

static void gb_i_bit_7_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->h, 7);
}

static void gb_i_bit_7_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_bit_set_flags(gb, &cpu->l, 7);
}

static void gb_i_bit_7_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_bit_set_flags(gb, &v, 7);
}

static void gb_cpu_res(struct gb *gb, uint8_t *v, unsigned bit) {
     *v = *v & ~(1U << bit);
}

static void gb_i_res_0_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->a, 0);
}

static void gb_i_res_0_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->b, 0);
}

static void gb_i_res_0_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->c, 0);
}

static void gb_i_res_0_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->d, 0);
}

static void gb_i_res_0_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->e, 0);
}

static void gb_i_res_0_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->h, 0);
}

static void gb_i_res_0_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->l, 0);
}

static void gb_i_res_0_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_res(gb, &v, 0);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_i_res_1_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->a, 1);
}

static void gb_i_res_1_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->b, 1);
}

static void gb_i_res_1_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->c, 1);
}

static void gb_i_res_1_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->d, 1);
}

static void gb_i_res_1_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->e, 1);
}

static void gb_i_res_1_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->h, 1);
}

static void gb_i_res_1_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->l, 1);
}

static void gb_i_res_1_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_res(gb, &v, 1);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_i_res_2_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->a, 2);
}

static void gb_i_res_2_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->b, 2);
}

static void gb_i_res_2_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->c, 2);
}

static void gb_i_res_2_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->d, 2);
}

static void gb_i_res_2_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->e, 2);
}

static void gb_i_res_2_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->h, 2);
}

static void gb_i_res_2_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->l, 2);
}

static void gb_i_res_2_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_res(gb, &v, 2);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_i_res_3_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->a, 3);
}

static void gb_i_res_3_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->b, 3);
}

static void gb_i_res_3_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->c, 3);
}

static void gb_i_res_3_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->d, 3);
}

static void gb_i_res_3_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->e, 3);
}

static void gb_i_res_3_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->h, 3);
}

static void gb_i_res_3_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->l, 3);
}

static void gb_i_res_3_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_res(gb, &v, 3);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_i_res_4_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->a, 4);
}

static void gb_i_res_4_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->b, 4);
}

static void gb_i_res_4_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->c, 4);
}

static void gb_i_res_4_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->d, 4);
}

static void gb_i_res_4_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->e, 4);
}

static void gb_i_res_4_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->h, 4);
}

static void gb_i_res_4_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->l, 4);
}

static void gb_i_res_4_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_res(gb, &v, 4);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_i_res_5_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->a, 5);
}

static void gb_i_res_5_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->b, 5);
}

static void gb_i_res_5_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->c, 5);
}

static void gb_i_res_5_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->d, 5);
}

static void gb_i_res_5_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->e, 5);
}

static void gb_i_res_5_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->h, 5);
}

static void gb_i_res_5_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->l, 5);
}

static void gb_i_res_5_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_res(gb, &v, 5);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_i_res_6_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->a, 6);
}

static void gb_i_res_6_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->b, 6);
}

static void gb_i_res_6_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->c, 6);
}

static void gb_i_res_6_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->d, 6);
}

static void gb_i_res_6_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->e, 6);
}

static void gb_i_res_6_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->h, 6);
}

static void gb_i_res_6_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->l, 6);
}

static void gb_i_res_6_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_res(gb, &v, 6);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_i_res_7_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->a, 7);
}

static void gb_i_res_7_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->b, 7);
}

static void gb_i_res_7_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->c, 7);
}

static void gb_i_res_7_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->d, 7);
}

static void gb_i_res_7_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->e, 7);
}

static void gb_i_res_7_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->h, 7);
}

static void gb_i_res_7_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_res(gb, &cpu->l, 7);
}

static void gb_i_res_7_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_res(gb, &v, 7);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_cpu_set(struct gb *gb, uint8_t *v, unsigned bit) {
     *v = *v | (1U << bit);
}

static void gb_i_set_0_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->a, 0);
}

static void gb_i_set_0_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->b, 0);
}

static void gb_i_set_0_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->c, 0);
}

static void gb_i_set_0_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->d, 0);
}

static void gb_i_set_0_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->e, 0);
}

static void gb_i_set_0_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->h, 0);
}

static void gb_i_set_0_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->l, 0);
}

static void gb_i_set_0_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_set(gb, &v, 0);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_i_set_1_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->a, 1);
}

static void gb_i_set_1_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->b, 1);
}

static void gb_i_set_1_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->c, 1);
}

static void gb_i_set_1_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->d, 1);
}

static void gb_i_set_1_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->e, 1);
}

static void gb_i_set_1_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->h, 1);
}

static void gb_i_set_1_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->l, 1);
}

static void gb_i_set_1_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_set(gb, &v, 1);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_i_set_2_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->a, 2);
}

static void gb_i_set_2_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->b, 2);
}

static void gb_i_set_2_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->c, 2);
}

static void gb_i_set_2_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->d, 2);
}

static void gb_i_set_2_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->e, 2);
}

static void gb_i_set_2_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->h, 2);
}

static void gb_i_set_2_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->l, 2);
}

static void gb_i_set_2_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_set(gb, &v, 2);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_i_set_3_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->a, 3);
}

static void gb_i_set_3_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->b, 3);
}

static void gb_i_set_3_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->c, 3);
}

static void gb_i_set_3_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->d, 3);
}

static void gb_i_set_3_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->e, 3);
}

static void gb_i_set_3_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->h, 3);
}

static void gb_i_set_3_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->l, 3);
}

static void gb_i_set_3_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_set(gb, &v, 3);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_i_set_4_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->a, 4);
}

static void gb_i_set_4_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->b, 4);
}

static void gb_i_set_4_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->c, 4);
}

static void gb_i_set_4_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->d, 4);
}

static void gb_i_set_4_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->e, 4);
}

static void gb_i_set_4_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->h, 4);
}

static void gb_i_set_4_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->l, 4);
}

static void gb_i_set_4_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_set(gb, &v, 4);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_i_set_5_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->a, 5);
}

static void gb_i_set_5_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->b, 5);
}

static void gb_i_set_5_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->c, 5);
}

static void gb_i_set_5_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->d, 5);
}

static void gb_i_set_5_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->e, 5);
}

static void gb_i_set_5_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->h, 5);
}

static void gb_i_set_5_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->l, 5);
}

static void gb_i_set_5_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_set(gb, &v, 5);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_i_set_6_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->a, 6);
}

static void gb_i_set_6_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->b, 6);
}

static void gb_i_set_6_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->c, 6);
}

static void gb_i_set_6_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->d, 6);
}

static void gb_i_set_6_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->e, 6);
}

static void gb_i_set_6_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->h, 6);
}

static void gb_i_set_6_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->l, 6);
}

static void gb_i_set_6_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_set(gb, &v, 6);

     gb_cpu_writeb(gb, hl, v);
}

static void gb_i_set_7_a(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->a, 7);
}

static void gb_i_set_7_b(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->b, 7);
}

static void gb_i_set_7_c(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->c, 7);
}

static void gb_i_set_7_d(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->d, 7);
}

static void gb_i_set_7_e(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->e, 7);
}

static void gb_i_set_7_h(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->h, 7);
}

static void gb_i_set_7_l(struct gb *gb) {
     struct gb_cpu *cpu = &gb->cpu;

     gb_cpu_set(gb, &cpu->l, 7);
}

static void gb_i_set_7_mhl(struct gb *gb) {
     uint16_t hl = gb_cpu_hl(gb);
     uint8_t v;

     v = gb_cpu_readb(gb, hl);

     gb_cpu_set(gb, &v, 7);

     gb_cpu_writeb(gb, hl, v);
}

static gb_instruction_f gb_instructions_cb[0x100] = {
     // 0x00
     gb_i_rlc_b,
     gb_i_rlc_c,
     gb_i_rlc_d,
     gb_i_rlc_e,
     gb_i_rlc_h,
     gb_i_rlc_l,
     gb_i_rlc_mhl,
     gb_i_rlc_a,
     gb_i_rrc_b,
     gb_i_rrc_c,
     gb_i_rrc_d,
     gb_i_rrc_e,
     gb_i_rrc_h,
     gb_i_rrc_l,
     gb_i_rrc_mhl,
     gb_i_rrc_a,
     // 0x10
     gb_i_rl_b,
     gb_i_rl_c,
     gb_i_rl_d,
     gb_i_rl_e,
     gb_i_rl_h,
     gb_i_rl_l,
     gb_i_rl_mhl,
     gb_i_rl_a,
     gb_i_rr_b,
     gb_i_rr_c,
     gb_i_rr_d,
     gb_i_rr_e,
     gb_i_rr_h,
     gb_i_rr_l,
     gb_i_rr_mhl,
     gb_i_rr_a,
     // 0x20
     gb_i_sla_b,
     gb_i_sla_c,
     gb_i_sla_d,
     gb_i_sla_e,
     gb_i_sla_h,
     gb_i_sla_l,
     gb_i_sla_mhl,
     gb_i_sla_a,
     gb_i_sra_b,
     gb_i_sra_c,
     gb_i_sra_d,
     gb_i_sra_e,
     gb_i_sra_h,
     gb_i_sra_l,
     gb_i_sra_mhl,
     gb_i_sra_a,
     // 0x30
     gb_i_swap_b,
     gb_i_swap_c,
     gb_i_swap_d,
     gb_i_swap_e,
     gb_i_swap_h,
     gb_i_swap_l,
     gb_i_swap_mhl,
     gb_i_swap_a,
     gb_i_srl_b,
     gb_i_srl_c,
     gb_i_srl_d,
     gb_i_srl_e,
     gb_i_srl_h,
     gb_i_srl_l,
     gb_i_srl_mhl,
     gb_i_srl_a,
     // 0x40
     gb_i_bit_0_b,
     gb_i_bit_0_c,
     gb_i_bit_0_d,
     gb_i_bit_0_e,
     gb_i_bit_0_h,
     gb_i_bit_0_l,
     gb_i_bit_0_mhl,
     gb_i_bit_0_a,
     gb_i_bit_1_b,
     gb_i_bit_1_c,
     gb_i_bit_1_d,
     gb_i_bit_1_e,
     gb_i_bit_1_h,
     gb_i_bit_1_l,
     gb_i_bit_1_mhl,
     gb_i_bit_1_a,
     // 0x50
     gb_i_bit_2_b,
     gb_i_bit_2_c,
     gb_i_bit_2_d,
     gb_i_bit_2_e,
     gb_i_bit_2_h,
     gb_i_bit_2_l,
     gb_i_bit_2_mhl,
     gb_i_bit_2_a,
     gb_i_bit_3_b,
     gb_i_bit_3_c,
     gb_i_bit_3_d,
     gb_i_bit_3_e,
     gb_i_bit_3_h,
     gb_i_bit_3_l,
     gb_i_bit_3_mhl,
     gb_i_bit_3_a,
     // 0x60
     gb_i_bit_4_b,
     gb_i_bit_4_c,
     gb_i_bit_4_d,
     gb_i_bit_4_e,
     gb_i_bit_4_h,
     gb_i_bit_4_l,
     gb_i_bit_4_mhl,
     gb_i_bit_4_a,
     gb_i_bit_5_b,
     gb_i_bit_5_c,
     gb_i_bit_5_d,
     gb_i_bit_5_e,
     gb_i_bit_5_h,
     gb_i_bit_5_l,
     gb_i_bit_5_mhl,
     gb_i_bit_5_a,
     // 0x70
     gb_i_bit_6_b,
     gb_i_bit_6_c,
     gb_i_bit_6_d,
     gb_i_bit_6_e,
     gb_i_bit_6_h,
     gb_i_bit_6_l,
     gb_i_bit_6_mhl,
     gb_i_bit_6_a,
     gb_i_bit_7_b,
     gb_i_bit_7_c,
     gb_i_bit_7_d,
     gb_i_bit_7_e,
     gb_i_bit_7_h,
     gb_i_bit_7_l,
     gb_i_bit_7_mhl,
     gb_i_bit_7_a,
     // 0x80
     gb_i_res_0_b,
     gb_i_res_0_c,
     gb_i_res_0_d,
     gb_i_res_0_e,
     gb_i_res_0_h,
     gb_i_res_0_l,
     gb_i_res_0_mhl,
     gb_i_res_0_a,
     gb_i_res_1_b,
     gb_i_res_1_c,
     gb_i_res_1_d,
     gb_i_res_1_e,
     gb_i_res_1_h,
     gb_i_res_1_l,
     gb_i_res_1_mhl,
     gb_i_res_1_a,
     // 0x90
     gb_i_res_2_b,
     gb_i_res_2_c,
     gb_i_res_2_d,
     gb_i_res_2_e,
     gb_i_res_2_h,
     gb_i_res_2_l,
     gb_i_res_2_mhl,
     gb_i_res_2_a,
     gb_i_res_3_b,
     gb_i_res_3_c,
     gb_i_res_3_d,
     gb_i_res_3_e,
     gb_i_res_3_h,
     gb_i_res_3_l,
     gb_i_res_3_mhl,
     gb_i_res_3_a,
     // 0xa0
     gb_i_res_4_b,
     gb_i_res_4_c,
     gb_i_res_4_d,
     gb_i_res_4_e,
     gb_i_res_4_h,
     gb_i_res_4_l,
     gb_i_res_4_mhl,
     gb_i_res_4_a,
     gb_i_res_5_b,
     gb_i_res_5_c,
     gb_i_res_5_d,
     gb_i_res_5_e,
     gb_i_res_5_h,
     gb_i_res_5_l,
     gb_i_res_5_mhl,
     gb_i_res_5_a,
     // 0xb0
     gb_i_res_6_b,
     gb_i_res_6_c,
     gb_i_res_6_d,
     gb_i_res_6_e,
     gb_i_res_6_h,
     gb_i_res_6_l,
     gb_i_res_6_mhl,
     gb_i_res_6_a,
     gb_i_res_7_b,
     gb_i_res_7_c,
     gb_i_res_7_d,
     gb_i_res_7_e,
     gb_i_res_7_h,
     gb_i_res_7_l,
     gb_i_res_7_mhl,
     gb_i_res_7_a,
     // 0xc0
     gb_i_set_0_b,
     gb_i_set_0_c,
     gb_i_set_0_d,
     gb_i_set_0_e,
     gb_i_set_0_h,
     gb_i_set_0_l,
     gb_i_set_0_mhl,
     gb_i_set_0_a,
     gb_i_set_1_b,
     gb_i_set_1_c,
     gb_i_set_1_d,
     gb_i_set_1_e,
     gb_i_set_1_h,
     gb_i_set_1_l,
     gb_i_set_1_mhl,
     gb_i_set_1_a,
     // 0xd0
     gb_i_set_2_b,
     gb_i_set_2_c,
     gb_i_set_2_d,
     gb_i_set_2_e,
     gb_i_set_2_h,
     gb_i_set_2_l,
     gb_i_set_2_mhl,
     gb_i_set_2_a,
     gb_i_set_3_b,
     gb_i_set_3_c,
     gb_i_set_3_d,
     gb_i_set_3_e,
     gb_i_set_3_h,
     gb_i_set_3_l,
     gb_i_set_3_mhl,
     gb_i_set_3_a,
     // 0xe0
     gb_i_set_4_b,
     gb_i_set_4_c,
     gb_i_set_4_d,
     gb_i_set_4_e,
     gb_i_set_4_h,
     gb_i_set_4_l,
     gb_i_set_4_mhl,
     gb_i_set_4_a,
     gb_i_set_5_b,
     gb_i_set_5_c,
     gb_i_set_5_d,
     gb_i_set_5_e,
     gb_i_set_5_h,
     gb_i_set_5_l,
     gb_i_set_5_mhl,
     gb_i_set_5_a,
     // 0xf0
     gb_i_set_6_b,
     gb_i_set_6_c,
     gb_i_set_6_d,
     gb_i_set_6_e,
     gb_i_set_6_h,
     gb_i_set_6_l,
     gb_i_set_6_mhl,
     gb_i_set_6_a,
     gb_i_set_7_b,
     gb_i_set_7_c,
     gb_i_set_7_d,
     gb_i_set_7_e,
     gb_i_set_7_h,
     gb_i_set_7_l,
     gb_i_set_7_mhl,
     gb_i_set_7_a,
};

static void gb_i_op_cb(struct gb *gb) {
     // Opcode 0xCB is used as a prefix for a second opcode map
     uint8_t instruction = gb_cpu_next_i8(gb);

     gb_instructions_cb[instruction](gb);
}
