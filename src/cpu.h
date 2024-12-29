#include <stdbool.h>
#include <stdint.h>

#include "mmu.h"

// endianness
#define HI 1
#define LO 0

typedef union _reg {
    uint16_t pair;
    uint8_t hilo[2];
    struct {
        unsigned lo : 4;
        unsigned c : 1;
        unsigned h : 1;
        unsigned n : 1;
        unsigned z : 1;
        uint8_t hi;
    } flags;
} reg;

typedef struct {
    bool halt, ime;
    uint16_t pc, sp;
    reg af, bc, de, hl;
    _mmu *mmu;
} sm83;

void sm83_init(sm83 *self, uint8_t *bootrom, uint8_t *rom);
void sm83_deinit(sm83 *self);
uint8_t sm83_step(sm83 *self);
