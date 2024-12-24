#include <stdint.h>
#include <stdio.h>

typedef struct { // we will likely need mappers here as well
    uint8_t *mem;
    uint8_t *rom;
} _mmu;

void mmu_init(_mmu *self, uint32_t memsize, FILE *bootrom_ptr, FILE *romptr);
void mmu_deinit(_mmu *self);

uint8_t mmu_read8(_mmu *self, uint16_t addr);
void mmu_write8(_mmu *self, uint16_t addr, uint8_t val);

// this is done in little endian
uint16_t mmu_read16(_mmu *self, uint16_t addr);
void mmu_write16(_mmu *self, uint16_t addr, uint16_t val);
