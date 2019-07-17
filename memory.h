#ifndef _GB_MEMORY_H_
#define _GB_MEMORY_H_

uint8_t gb_memory_readb(struct gb *gb, uint16_t addr);
void    gb_memory_writeb(struct gb *gb, uint16_t addr, uint8_t val);

#endif /* _GB_MEMORY_H_ */
