#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmu.h"

void mmu_init(_mmu *self, uint32_t memsize, FILE *bootrom_ptr, FILE *rom_ptr) {
    self->mem = (uint8_t *)malloc(memsize);
    self->rom = (uint8_t *)malloc(0x100);
    fread(self->mem, 1, 0x4000, rom_ptr);
    // load bootrom_ptr over the rom and unmap later
    if (bootrom_ptr) {
        memcpy(self->rom, self->mem, 0x100); // keep the first 256 bytes of the cartridge until we unmap the bootrom
        fread(self->mem, 1, 0x100, bootrom_ptr);
    }
}

void mmu_deinit(_mmu *self) {
    free(self->mem);
    free(self->rom);
}

uint8_t mmu_read8(_mmu *self, uint16_t addr) {
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
                } else if (addr == 0xff01) {
                    // serial transfer
                    return self->mem[0xff01];
                } else if (addr == 0xff02) {
                    return self->mem[0xff02];
#ifdef DEBUG // print contents of serial port to terminal
                    fprintf(stderr, "%c", self->mem[addr]);
#endif
                } else if (addr == 0xff04) {
                    // divier register
                } else if (addr == 0xff05) {
                    // timer counter
                } else if (addr == 0xff06) {
                    // timer modulo
                } else if (addr == 0xff07) {
                    // timer control
                } else if (addr == 0xff0f) {
                    // interrupt flag
                } else if (addr < 0xff27 && addr > 0xff09) {
                    // audio
                } else if (addr < 0xff40 && addr > 0xff29) {
                    // wave pattern
                } else if (addr == 0xff40) {
                    // lcd control
                } else if (addr == 0xff41) {
                    // lcd status
                } else if (addr == 0xff42) {
                    // viewport y pos
                } else if (addr == 0xff43) {
                    // viewport x pos
                } else if (addr == 0xff44) {
                    // lcd y co ordinate
                } else if (addr == 0xff45) {
                    // lcd y compare
                } else if (addr == 0xff46) {
                    // oam dma source addr and start
                } else if (addr == 0xff47) {
                    // bg colour palette
                } else if (addr == 0xff48) {
                    // obj palette 0 data
                } else if (addr == 0xff49) {
                    // obj palette 1 data
                } else if (addr == 0xff4a) {
                    // window y pos
                } else if (addr == 0xff4b) {
                    // window x pos + 7
                } else if (addr == 0xff50) {
                    // i don't know what happens if you read here, time to guess!!
                    return 0xff;
                }
            } else if (addr < 0xffff) {
                // hram
            } else if (addr == 0xffff) {
                // interrupt enable register
            } else {
                fprintf(stderr, "warning: unrecognised i/o port \"%04x\" read", addr);
                return 0xff;
            }
            break;
        default:
            return 0xff;
    }
}

void mmu_write8(_mmu *self, uint16_t addr, uint8_t val) {
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
                } else if (addr == 0xff01) {
                    // serial transfer
                    self->mem[0xff01] = val;
                } else if (addr == 0xff02) {
#ifdef DEBUG // print contents of serial port to terminal
                    fprintf(stderr, "%c", val);
#endif
                    self->mem[0xff02] = val;
                } else if (addr == 0xff04) {
                    // divier register
                } else if (addr == 0xff05) {
                    // timer counter
                } else if (addr == 0xff06) {
                    // timer modulo
                } else if (addr == 0xff07) {
                    // timer control
                } else if (addr == 0xff0f) {
                    // interrupt flag
                } else if (addr < 0xff27 && addr > 0xff09) {
                    // audio
                } else if (addr < 0xff40 && addr > 0xff29) {
                    // wave pattern
                } else if (addr == 0xff40) {
                    // lcd control
                } else if (addr == 0xff41) {
                    // lcd status
                } else if (addr == 0xff42) {
                    // viewport y pos
                } else if (addr == 0xff43) {
                    // viewport x pos
                } else if (addr == 0xff44) {
                    // lcd y co ordinate
                } else if (addr == 0xff45) {
                    // lcd y compare
                } else if (addr == 0xff46) {
                    // oam dma source addr and start
                } else if (addr == 0xff47) {
                    // bg colour palette
                } else if (addr == 0xff48) {
                    // obj palette 0 data
                } else if (addr == 0xff49) {
                    // obj palette 1 data
                } else if (addr == 0xff4a) {
                    // window y pos
                } else if (addr == 0xff4b) {
                    // window x pos + 7
                } else if (addr == 0xff50) {
                    // set to non 0 to unmap boot rom
                    if (val > 0) {
                        memcpy(self->mem, self->rom, 0x100);
                    }
                }
            } else if (addr < 0xffff) {
                // hram
            } else if (addr == 0xffff) {
                // interrupt enable register
            } else {
                fprintf(stderr, "warning: unrecognised i/o port \"%04x\" write", addr);
            }
            break;
        default:
            break;
    }
}

// this is done in little endian
inline uint16_t mmu_read16(_mmu *self, uint16_t addr) {
    return (mmu_read8(self, addr + 1) << 8) | mmu_read8(self, addr);
}

inline void mmu_write16(_mmu *self, uint16_t addr, uint16_t val) {
    mmu_write8(self, addr, val & 0xff);
    mmu_write8(self, addr + 1, (val & 0xff00) >> 8);
}
