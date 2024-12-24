#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "cpu.h"
#include "mmu.h"

inline uint8_t add8(sm83 *self, uint8_t b, bool carry) {
    self->af.flags.n = 0;
    self->af.flags.c = ((self->af.hilo[0] + b + carry) >> 8) & 1;
    self->af.flags.h = (((self->af.hilo[0] & 0xf) + (b & 0xf) + carry) >> 4) & 1;
    self->af.flags.z = (self->af.hilo[0] + b + carry) == 0;
    return self->af.hilo[0] + b;
}

inline uint8_t sub8(sm83 *self, uint8_t b, bool carry) {
    uint8_t result = add8(self, ~b, !carry);
    self->af.flags.n = 1;
    self->af.flags.c = !self->af.flags.c;
    return result;
}

inline uint8_t and8(sm83 *self, uint8_t b) {
    self->af.flags.n = 0;
    self->af.flags.c = 0;
    self->af.flags.h = 1;
    self->af.flags.z = (self->af.hilo[0] & b) == 0;
    return self->af.hilo[0] & b;
}

inline uint8_t or8(sm83 *self, uint8_t b) {
    self->af.flags.n = 0;
    self->af.flags.c = 0;
    self->af.flags.h = 0;
    self->af.flags.z = (self->af.hilo[0] | b) == 0;
    return self->af.hilo[0] | b;
}

inline uint8_t xor8(sm83 *self, uint8_t b) {
    self->af.flags.n = 0;
    self->af.flags.c = 0;
    self->af.flags.h = 0;
    self->af.flags.z = (self->af.hilo[0] ^ b) == 0;
    return self->af.hilo[0] ^ b;
}

void sm83_init(sm83 *self, int memsize) {
    self->af.pair = 0;
    self->bc.pair = 0;
    self->de.pair = 0;
    self->hl.pair = 0;
    self->sp = 0;
    self->pc = 0;
    self->halt = false;
    self->mem = (uint8_t *)malloc(memsize);
}

void sm83_deinit(sm83 *self) {
    free(self->mem);
}

void sm83_step(sm83 *self) {
    const uint8_t opcode = mmu_read8(self->mem, ++self->pc);
    switch (opcode) {
        case 0x00: // nop
            break;
        // ld xx, n16
        case 0x01:
            self->bc.pair = mmu_read16(self->mem, ++self->pc);
            ++self->pc;
            break;
        case 0x11:
            self->de.pair = mmu_read16(self->mem, ++self->pc);
            ++self->pc;
            break;
        case 0x21:
            self->hl.pair = mmu_read16(self->mem, ++self->pc);
            ++self->pc;
            break;
        case 0x31:
            self->sp = mmu_read16(self->mem, ++self->pc);
            ++self->pc;
            break;

        // ld [xx], a
        case 0x02:
            mmu_write8(self->mem, self->bc.pair, self->af.hilo[0]);
            break;
        case 0x12:
            mmu_write8(self->mem, self->de.pair, self->af.hilo[0]);
            break;
        case 0x22: // hl+
            mmu_write8(self->mem, self->hl.pair++, self->af.hilo[0]);
            break;
        case 0x32: // hl-
            mmu_write8(self->mem, self->hl.pair--, self->af.hilo[0]);
            break;

        // inc xx
        case 0x03:
            ++self->bc.pair;
            break;
        case 0x13:
            ++self->de.pair;
            break;
        case 0x23:
            ++self->hl.pair;
            break;
        case 0x33:
            ++self->sp;
            break;

        // inc x
        case 0x04:
            ++self->bc.hilo[0];
            self->af.flags.z = self->bc.hilo[0] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->bc.hilo[0] & 0xf) == 0;
            break;
        case 0x14:
            ++self->de.hilo[0];
            self->af.flags.z = self->de.hilo[0] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->de.hilo[0] & 0xf) == 0;
            break;
        case 0x24:
            ++self->hl.hilo[0];
            self->af.flags.z = self->hl.hilo[0] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->hl.hilo[0] & 0xf) == 0;
            break;
        case 0x34:
            ++self->mem[self->hl.pair];
            self->af.flags.z = self->mem[self->hl.pair] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->mem[self->hl.pair] & 0xf) == 0;
            break;

        // dec x
        case 0x05:
            --self->bc.hilo[0];
            self->af.flags.z = self->bc.hilo[0] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = !((self->bc.hilo[0] & 0xf) == 0);
            break;
        case 0x15:
            --self->de.hilo[0];
            self->af.flags.z = self->de.hilo[0] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = !((self->de.hilo[0] & 0xf) == 0);
            break;
        case 0x25:
            --self->hl.hilo[0];
            self->af.flags.z = self->hl.hilo[0] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = !((self->hl.hilo[0] & 0xf) == 0);
            break;
        case 0x35:
            --self->mem[self->hl.pair];
            self->af.flags.z = self->mem[self->hl.pair] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = !((self->mem[self->hl.pair] & 0xf) == 0);
            break;

        // ld x, n8
        case 0x06:
            self->bc.hilo[0] = mmu_read8(self->mem, ++self->pc);
            break;
        case 0x16:
            self->de.hilo[0] = mmu_read8(self->mem, ++self->pc);
            break;
        case 0x26:
            self->hl.hilo[0] = mmu_read8(self->mem, ++self->pc);
            break;
        case 0x36:
            mmu_write8(self->mem, self->hl.pair, mmu_read8(self->mem, ++self->pc));
            break;

        // add hl, xx
        case 0x09:
            self->af.flags.n = 0;
            self->af.flags.h = (((self->hl.pair & 0xfff) + (self->bc.pair & 0xfff)) >> 12) & 1;
            self->af.flags.c = ((self->hl.pair + self->bc.pair) >> 16) & 1;
            self->hl.pair += self->bc.pair;
            break;
        case 0x19:
            self->af.flags.n = 0;
            self->af.flags.h = (((self->hl.pair & 0xfff) + (self->de.pair & 0xfff)) >> 12) & 1;
            self->af.flags.c = ((self->hl.pair + self->de.pair) >> 16) & 1;
            self->hl.pair += self->de.pair;
            break;
        case 0x29:
            self->af.flags.n = 0;
            self->af.flags.h = (((self->hl.pair & 0xfff) + (self->hl.pair & 0xfff)) >> 12) & 1;
            self->af.flags.c = ((self->hl.pair + self->hl.pair) >> 16) & 1;
            self->hl.pair += self->hl.pair;
            break;
        case 0x39:
            self->af.flags.n = 0;
            self->af.flags.h = (((self->hl.pair & 0xfff) + (self->sp & 0xfff)) >> 12) & 1;
            self->af.flags.c = ((self->hl.pair + self->sp) >> 16) & 1;
            self->hl.pair += self->sp;
            break;

        // ld a, [xx]
        case 0x0a:
            self->af.hilo[0] = mmu_read8(self->mem, self->bc.pair);
            break;
        case 0x1a:
            self->af.hilo[0] = mmu_read8(self->mem, self->de.pair);
            break;
        case 0x2a:
            self->af.hilo[0] = mmu_read8(self->mem, self->hl.pair++);
            break;
        case 0x3a:
            self->af.hilo[0] = mmu_read8(self->mem, self->hl.pair--);
            break;

        // dec xx
        case 0x0b:
            --self->bc.pair;
            break;
        case 0x1b:
            --self->de.pair;
            break;
        case 0x2b:
            --self->hl.pair;
            break;
        case 0x3b:
            --self->sp;
            break;

        // inc x
        case 0x0c:
            ++self->bc.hilo[1];
            self->af.flags.z = self->bc.hilo[1] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->bc.hilo[1] & 0xf) == 0;
            break;
        case 0x1c:
            ++self->de.hilo[1];
            self->af.flags.z = self->de.hilo[1] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->de.hilo[1] & 0xf) == 0;
            break;
        case 0x2c:
            ++self->hl.hilo[1];
            self->af.flags.z = self->hl.hilo[1] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->hl.hilo[1] & 0xf) == 0;
            break;
        case 0x3c:
            ++self->af.hilo[0];
            self->af.flags.z = self->af.hilo[0] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->af.hilo[0] & 0xf) == 0;
            break;

        // dec x
        case 0x0d:
            --self->bc.hilo[1];
            self->af.flags.z = self->bc.hilo[1] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = !((self->bc.hilo[1] & 0xf) == 0);
            break;
        case 0x1d:
            --self->de.hilo[1];
            self->af.flags.z = self->de.hilo[1] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = !((self->de.hilo[1] & 0xf) == 0);
            break;
        case 0x2d:
            --self->hl.hilo[1];
            self->af.flags.z = self->hl.hilo[1] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = !((self->hl.hilo[1] & 0xf) == 0);
            break;
        case 0x3d:
            --self->af.hilo[0];
            self->af.flags.z = self->af.hilo[0] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = !((self->af.hilo[0] & 0xf) == 0);
            break;

        // ld x, n8
        case 0x0e:
            self->bc.hilo[1] = mmu_read8(self->mem, ++self->pc);
            break;
        case 0x1e:
            self->de.hilo[1] = mmu_read8(self->mem, ++self->pc);
            break;
        case 0x2e:
            self->hl.hilo[1] = mmu_read8(self->mem, ++self->pc);
            break;
        case 0x3e:
            self->af.hilo[0] = mmu_read8(self->mem, ++self->pc);
            break;

        // ld x, x
        case 0x40:
            self->bc.hilo[0] = self->bc.hilo[0];
            break;
        case 0x41:
            self->bc.hilo[0] = self->bc.hilo[1];
            break;
        case 0x42:
            self->bc.hilo[0] = self->de.hilo[0];
            break;
        case 0x43:
            self->bc.hilo[0] = self->de.hilo[1];
            break;
        case 0x44:
            self->bc.hilo[0] = self->hl.hilo[0];
            break;
        case 0x45:
            self->bc.hilo[0] = self->hl.hilo[1];
            break;
        case 0x46:
            self->bc.hilo[0] = mmu_read8(self->mem, self->hl.pair);
            break;
        case 0x47:
            self->bc.hilo[0] = self->af.hilo[0];
            break;
        case 0x48:
            self->bc.hilo[1] = self->bc.hilo[0];
            break;
        case 0x49:
            self->bc.hilo[1] = self->bc.hilo[1];
            break;
        case 0x4a:
            self->bc.hilo[1] = self->de.hilo[0];
            break;
        case 0x4b:
            self->bc.hilo[1] = self->de.hilo[1];
            break;
        case 0x4c:
            self->bc.hilo[1] = self->hl.hilo[0];
            break;
        case 0x4d:
            self->bc.hilo[1] = self->hl.hilo[1];
            break;
        case 0x4e:
            self->bc.hilo[1] = mmu_read8(self->mem, self->hl.pair);
            break;
        case 0x4f:
            self->bc.hilo[1] = self->af.hilo[0];
            break;
        case 0x50:
            self->de.hilo[0] = self->bc.hilo[0];
            break;
        case 0x51:
            self->de.hilo[0] = self->bc.hilo[1];
            break;
        case 0x52:
            self->de.hilo[0] = self->de.hilo[0];
            break;
        case 0x53:
            self->de.hilo[0] = self->de.hilo[1];
            break;
        case 0x54:
            self->de.hilo[0] = self->hl.hilo[0];
            break;
        case 0x55:
            self->de.hilo[0] = self->hl.hilo[1];
            break;
        case 0x56:
            self->de.hilo[0] = mmu_read8(self->mem, self->hl.pair);
            break;
        case 0x57:
            self->de.hilo[0] = self->af.hilo[0];
            break;
        case 0x58:
            self->de.hilo[1] = self->bc.hilo[0];
            break;
        case 0x59:
            self->de.hilo[1] = self->bc.hilo[1];
            break;
        case 0x5a:
            self->de.hilo[1] = self->de.hilo[0];
            break;
        case 0x5b:
            self->de.hilo[1] = self->de.hilo[1];
            break;
        case 0x5c:
            self->de.hilo[1] = self->hl.hilo[0];
            break;
        case 0x5d:
            self->de.hilo[1] = self->hl.hilo[1];
            break;
        case 0x5e:
            self->de.hilo[1] = mmu_read8(self->mem, self->hl.pair);
            break;
        case 0x5f:
            self->de.hilo[1] = self->af.hilo[0];
            break;
        case 0x60:
            self->hl.hilo[0] = self->bc.hilo[0];
            break;
        case 0x61:
            self->hl.hilo[0] = self->bc.hilo[1];
            break;
        case 0x62:
            self->hl.hilo[0] = self->de.hilo[0];
            break;
        case 0x63:
            self->hl.hilo[0] = self->de.hilo[1];
            break;
        case 0x64:
            self->hl.hilo[0] = self->hl.hilo[0];
            break;
        case 0x65:
            self->hl.hilo[0] = self->hl.hilo[1];
            break;
        case 0x66:
            self->hl.hilo[0] = mmu_read8(self->mem, self->hl.pair);
            break;
        case 0x67:
            self->hl.hilo[0] = self->af.hilo[0];
            break;
        case 0x68:
            self->hl.hilo[1] = self->bc.hilo[0];
            break;
        case 0x69:
            self->hl.hilo[1] = self->bc.hilo[1];
            break;
        case 0x6a:
            self->hl.hilo[1] = self->de.hilo[0];
            break;
        case 0x6b:
            self->hl.hilo[1] = self->de.hilo[1];
            break;
        case 0x6c:
            self->hl.hilo[1] = self->hl.hilo[0];
            break;
        case 0x6d:
            self->hl.hilo[1] = self->hl.hilo[1];
            break;
        case 0x6e:
            self->hl.hilo[1] = mmu_read8(self->mem, self->hl.pair);
            break;
        case 0x6f:
            self->hl.hilo[1] = self->af.hilo[0];
            break;
        case 0x70:
            mmu_write8(self->mem, self->hl.pair, self->bc.hilo[0]);
            break;
        case 0x71:
            mmu_write8(self->mem, self->hl.pair, self->bc.hilo[1]);
            break;
        case 0x72:
            mmu_write8(self->mem, self->hl.pair, self->de.hilo[0]);
            break;
        case 0x73:
            mmu_write8(self->mem, self->hl.pair, self->de.hilo[1]);
            break;
        case 0x74:
            mmu_write8(self->mem, self->hl.pair, self->hl.hilo[0]);
            break;
        case 0x75:
            mmu_write8(self->mem, self->hl.pair, self->hl.hilo[1]);
            break;
        case 0x76:
            self->halt = true;
            break;
        case 0x77:
            mmu_write8(self->mem, self->hl.pair, self->af.hilo[0]);
            break;
        case 0x78:
            self->af.hilo[0] = self->bc.hilo[0];
            break;
        case 0x79:
            self->af.hilo[0] = self->bc.hilo[1];
            break;
        case 0x7a:
            self->af.hilo[0] = self->de.hilo[0];
            break;
        case 0x7b:
            self->af.hilo[0] = self->de.hilo[1];
            break;
        case 0x7c:
            self->af.hilo[0] = self->hl.hilo[0];
            break;
        case 0x7d:
            self->af.hilo[0] = self->hl.hilo[1];
            break;
        case 0x7e:
            self->af.hilo[0] = mmu_read8(self->mem, self->hl.pair);
            break;
        case 0x7f:
            self->af.hilo[0] = self->af.hilo[0];
            break;

        // logical instructions
        // add
        case 0x80:
            self->af.hilo[0] = add8(self, self->bc.hilo[0], 0);
            break;
        case 0x81:
            self->af.hilo[0] = add8(self, self->bc.hilo[1], 0);
            break;
        case 0x82:
            self->af.hilo[0] = add8(self, self->de.hilo[0], 0);
            break;
        case 0x83:
            self->af.hilo[0] = add8(self, self->de.hilo[1], 0);
            break;
        case 0x84:
            self->af.hilo[0] = add8(self, self->hl.hilo[0], 0);
            break;
        case 0x85:
            self->af.hilo[0] = add8(self, self->hl.hilo[1], 0);
            break;
        case 0x86:
            self->af.hilo[0] = add8(self, mmu_read8(self->mem, self->hl.pair), 0);
            break;
        case 0x87:
            self->af.hilo[0] = add8(self, self->af.hilo[0], 0);
            break;
        // adc
        case 0x88:
            self->af.hilo[0] = add8(self, self->bc.hilo[0], self->af.flags.c);
            break;
        case 0x89:
            self->af.hilo[0] = add8(self, self->bc.hilo[1], self->af.flags.c);
            break;
        case 0x8a:
            self->af.hilo[0] = add8(self, self->de.hilo[0], self->af.flags.c);
            break;
        case 0x8b:
            self->af.hilo[0] = add8(self, self->de.hilo[1], self->af.flags.c);
            break;
        case 0x8c:
            self->af.hilo[0] = add8(self, self->hl.hilo[0], self->af.flags.c);
            break;
        case 0x8d:
            self->af.hilo[0] = add8(self, self->hl.hilo[1], self->af.flags.c);
            break;
        case 0x8e:
            self->af.hilo[0] = add8(self, mmu_read8(self->mem, self->hl.pair), self->af.flags.c);
            break;
        case 0x8f:
            self->af.hilo[0] = add8(self, self->af.hilo[0], self->af.flags.c);
            break;
        // sub
        case 0x90:
            self->af.hilo[0] = sub8(self, self->bc.hilo[0], 0);
            break;
        case 0x91:
            self->af.hilo[0] = sub8(self, self->bc.hilo[1], 0);
            break;
        case 0x92:
            self->af.hilo[0] = sub8(self, self->de.hilo[0], 0);
            break;
        case 0x93:
            self->af.hilo[0] = sub8(self, self->de.hilo[1], 0);
            break;
        case 0x94:
            self->af.hilo[0] = sub8(self, self->hl.hilo[0], 0);
            break;
        case 0x95:
            self->af.hilo[0] = sub8(self, self->hl.hilo[1], 0);
            break;
        case 0x96:
            self->af.hilo[0] = sub8(self, mmu_read8(self->mem, self->hl.pair), 0);
            break;
        case 0x97:
            self->af.hilo[0] = sub8(self, self->af.hilo[0], 0);
            break;
        // sbc
        case 0x98:
            self->af.hilo[0] = sub8(self, self->bc.hilo[0], self->af.flags.c);
            break;
        case 0x99:
            self->af.hilo[0] = sub8(self, self->bc.hilo[1], self->af.flags.c);
            break;
        case 0x9a:
            self->af.hilo[0] = sub8(self, self->de.hilo[0], self->af.flags.c);
            break;
        case 0x9b:
            self->af.hilo[0] = sub8(self, self->de.hilo[1], self->af.flags.c);
            break;
        case 0x9c:
            self->af.hilo[0] = sub8(self, self->hl.hilo[0], self->af.flags.c);
            break;
        case 0x9d:
            self->af.hilo[0] = sub8(self, self->hl.hilo[1], self->af.flags.c);
            break;
        case 0x9e:
            self->af.hilo[0] = sub8(self, mmu_read8(self->mem, self->hl.pair), self->af.flags.c);
            break;
        case 0x9f:
            self->af.hilo[0] = sub8(self, self->af.hilo[0], self->af.flags.c);
            break;
        // and
        case 0xa0:
            self->af.hilo[0] = and8(self, self->bc.hilo[0]);
            break;
        case 0xa1:
            self->af.hilo[0] = and8(self, self->bc.hilo[1]);
            break;
        case 0xa2:
            self->af.hilo[0] = and8(self, self->de.hilo[0]);
            break;
        case 0xa3:
            self->af.hilo[0] = and8(self, self->de.hilo[1]);
            break;
        case 0xa4:
            self->af.hilo[0] = and8(self, self->hl.hilo[0]);
            break;
        case 0xa5:
            self->af.hilo[0] = and8(self, self->hl.hilo[1]);
            break;
        case 0xa6:
            self->af.hilo[0] = and8(self, mmu_read8(self->mem, self->hl.pair));
            break;
        case 0xa7:
            self->af.hilo[0] = and8(self, self->af.hilo[0]);
            break;
        // xor
        case 0xa8:
            self->af.hilo[0] = xor8(self, self->bc.hilo[0]);
            break;
        case 0xa9:
            self->af.hilo[0] = xor8(self, self->bc.hilo[1]);
            break;
        case 0xaa:
            self->af.hilo[0] = xor8(self, self->de.hilo[0]);
            break;
        case 0xab:
            self->af.hilo[0] = xor8(self, self->de.hilo[1]);
            break;
        case 0xac:
            self->af.hilo[0] = xor8(self, self->hl.hilo[0]);
            break;
        case 0xad:
            self->af.hilo[0] = xor8(self, self->hl.hilo[1]);
            break;
        case 0xae:
            self->af.hilo[0] = xor8(self, mmu_read8(self->mem, self->hl.pair));
            break;
        case 0xaf:
            self->af.hilo[0] = xor8(self, self->af.hilo[0]);
            break;
        // or
        case 0xb0:
            self->af.hilo[0] = or8(self, self->bc.hilo[0]);
            break;
        case 0xb1:
            self->af.hilo[0] = or8(self, self->bc.hilo[1]);
            break;
        case 0xb2:
            self->af.hilo[0] = or8(self, self->de.hilo[0]);
            break;
        case 0xb3:
            self->af.hilo[0] = or8(self, self->de.hilo[1]);
            break;
        case 0xb4:
            self->af.hilo[0] = or8(self, self->hl.hilo[0]);
            break;
        case 0xb5:
            self->af.hilo[0] = or8(self, self->hl.hilo[1]);
            break;
        case 0xb6:
            self->af.hilo[0] = or8(self, mmu_read8(self->mem, self->hl.pair));
            break;
        case 0xb7:
            self->af.hilo[0] = or8(self, self->af.hilo[0]);
            break;
        // cp
        case 0xb8:
            sub8(self, self->bc.hilo[0], self->af.flags.c);
            break;
        case 0xb9:
            sub8(self, self->bc.hilo[1], self->af.flags.c);
            break;
        case 0xba:
            sub8(self, self->de.hilo[0], self->af.flags.c);
            break;
        case 0xbb:
            sub8(self, self->de.hilo[1], self->af.flags.c);
            break;
        case 0xbc:
            sub8(self, self->hl.hilo[0], self->af.flags.c);
            break;
        case 0xbd:
            sub8(self, self->hl.hilo[1], self->af.flags.c);
            break;
        case 0xbe:
            sub8(self, mmu_read8(self->mem, self->hl.pair), self->af.flags.c);
            break;
        case 0xbf:
            sub8(self, self->af.hilo[0], self->af.flags.c);
            break;

        // logic n8 instructions
        case 0xc6:
            self->af.hilo[0] = add8(self, mmu_read8(self->mem, ++self->pc), 0);
            break;
        case 0xce:
            self->af.hilo[0] = add8(self, mmu_read8(self->mem, ++self->pc), self->af.flags.c);
            break;
        case 0xd6:
            self->af.hilo[0] = sub8(self, mmu_read8(self->mem, ++self->pc), 0);
            break;
        case 0xde:
            self->af.hilo[0] = sub8(self, mmu_read8(self->mem, ++self->pc), self->af.flags.c);
            break;
        case 0xe6:
            self->af.hilo[0] = and8(self, mmu_read8(self->mem, ++self->pc));
            break;
        case 0xee:
            self->af.hilo[0] = xor8(self, mmu_read8(self->mem, ++self->pc));
            break;
        case 0xf6:
            self->af.hilo[0] = or8(self, mmu_read8(self->mem, ++self->pc));
            break;
        case 0xfe:
            sub8(self, mmu_read8(self->mem, ++self->pc), self->af.flags.c);
            break;
    }
}
