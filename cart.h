#ifndef _GB_CART_H_
#define _GB_CART_H_

enum gb_cart_model {
     /* No mapper: 2 ROM banks, no RAM */
     GB_CART_SIMPLE = 0,
};

struct gb_cart {
     /* Full ROM contents */
     uint8_t *rom;
     /* ROM length in bytes */
     unsigned rom_length;
     /* Type of cartridge */
     enum gb_cart_model model;
};

void gb_cart_load(struct gb *gb, const char *rom_path);
void gb_cart_unload(struct gb *gb);
uint8_t gb_cart_rom_readb(struct gb *gb, uint16_t addr);
void gb_cart_rom_writeb(struct gb *gb, uint16_t addr, uint8_t v);
uint8_t gb_cart_ram_readb(struct gb *gb, uint16_t addr);
void gb_cart_ram_writeb(struct gb *gb, uint16_t addr, uint8_t v);

#endif /* _GB_CART_H_ */
