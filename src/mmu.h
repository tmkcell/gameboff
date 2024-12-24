#include <stdint.h>

uint8_t mmu_read8(uint8_t *mem, uint16_t addr);
void mmu_write8(uint8_t *mem, uint16_t addr, uint8_t val);

// this is done in little endian
uint16_t mmu_read16(uint8_t *mem, uint16_t addr);
void mmu_write16(uint8_t *mem, uint16_t addr, uint16_t val);
