#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "mmu.h"

typedef union _reg {
    uint16_t pair;
    uint8_t hilo[2];
    struct {
        uint8_t hi;
        unsigned z : 1;
        unsigned n : 1;
        unsigned h : 1;
        unsigned c : 1;
        unsigned lo : 4;
    } flags;
} reg;

typedef struct {
    _mmu *mmu;
    reg af, bc, de, hl;
    uint16_t pc, sp;
    bool halt, interrupts;
} sm83;

void sm83_init(sm83 *self, uint32_t memsize, FILE *bootrom_ptr, FILE *rom_ptr);
void sm83_deinit(sm83 *self);
void sm83_step(sm83 *self);
