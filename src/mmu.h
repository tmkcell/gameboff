#include <stdint.h>
#include <stdio.h>

typedef struct { // we will likely need mappers here as well
    uint8_t mem[0x2000]; // 0x10000 in spirit but 0x8000 is rom so wasted space
    uint8_t rombank;
    uint8_t *rom;
#ifdef DEBUG
    uint8_t opcode;
#endif
} _mmu;

void mmu_init(_mmu *self, uint32_t romsize, FILE *bootrom_ptr, FILE *romptr);
void mmu_deinit(_mmu *self);

uint8_t mmu_read8(_mmu *self, uint16_t addr);
void mmu_write8(_mmu *self, uint16_t addr, uint8_t val);

// this is done in little endian
uint16_t mmu_read16(_mmu *self, uint16_t addr);
void mmu_write16(_mmu *self, uint16_t addr, uint16_t val);
