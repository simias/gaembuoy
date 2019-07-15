#ifndef _GB_CART_H_
#define _GB_CART_H_

struct gb_cart {
     uint8_t *rom;
     unsigned rom_length;
};

void gb_cart_load(struct gb *gb, const char *rom_path);
void gb_cart_unload(struct gb *gb);
uint8_t gb_cart_rom_readb(struct gb *gb, uint16_t addr);

#endif /* _GB_CART_H_ */
