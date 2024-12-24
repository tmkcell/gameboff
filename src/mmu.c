#include <stdint.h>
#include <stdio.h>

#include "mmu.h"

uint8_t mmu_read8(uint8_t *mem, uint16_t addr) {
    switch (addr & 0xf000) {
        case 0x0000:
            // rom bank 00
            break;
        case 0x4000:
            // rom bank 01-nn via mapper
            break;
        case 0x8000:
            // vram
            break;
        case 0xa000:
            // external ram
            break;
        case 0xc000:
            // wram
            break;
        case 0xd000:
            // wram, cgb can switch banks
            break;
        case 0xe000:
        echoram:
            break;
        case 0xf000:
            if (addr < 0xfe00)
                goto echoram;
            else if (addr < 0xfea0) {
                // oam
            } else if (addr < 0xff00) {
                // not useable
            } else if (addr < 0xff80) {
                // io registers excluding the cgb ones
                if (addr == 0xff00) {
                    // pad input
                } else if (addr < 0xff03) {
                    // serial transfer
                } else if (addr < 0xff08 && addr > 0xff03) {
                    // timer and divider
                } else if (addr == 0xff0f) {
                    // interrupts
                } else if (addr < 0xff27 && addr > 0xff09) {
                    // audio
                } else if (addr < 0xff40 && addr > 0xff29) {
                    // wave pattern
                } else if (addr == 0xff50) {
                    // set to non 0 to disable boot rom
                }
            } else if (addr < 0xffff) {
                // hram
            } else if (addr == 0xffff) {
                // interrupt enable register
            } else {
                fprintf(stderr, "warning: unrecognised i/o port \"%04x\" read", addr);
            }
            break;
    }
}

void mmu_write8(uint8_t *mem, uint16_t addr, uint8_t val) {
}

// this is done in little endian
uint16_t mmu_read16(uint8_t *mem, uint16_t addr) {
    return (mmu_read8(mem, addr + 1) << 8) | mmu_read8(mem, addr);
}

void mmu_write16(uint8_t *mem, uint16_t addr, uint16_t val) {
    mmu_write8(mem, addr, val & 0xff);
    mmu_write8(mem, addr + 1, (val & 0xff00) >> 8);
}
