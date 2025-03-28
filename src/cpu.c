#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef DEBUG
#include <stdio.h>
#include <unistd.h>
#endif

#include "cpu.h"

void sm83_init(sm83 *self, uint8_t *bootrom, uint8_t *rom) {
    if (bootrom) {
        self->pc = 0;
        self->af.pair = 0;
        self->bc.pair = 0;
        self->de.pair = 0;
        self->hl.pair = 0;
        self->sp = 0;
    } else { // emulate state after bootrom
        self->pc = 0x100;
        self->af.pair = 0x01b0;
        self->bc.pair = 0x0013;
        self->de.pair = 0x00d8;
        self->hl.pair = 0x014d;
        self->sp = 0xfffe;
    }
    self->halt = false;
    self->ime = false; // guessing it will be off on startup
    self->mmu = (_mmu *)malloc(sizeof(_mmu));
    mmu_init(self->mmu, bootrom, rom);
}

void sm83_deinit(sm83 *self) {
    free(self->mmu); // the rom is not allocated here so don't free
}

inline uint8_t add8(sm83 *self, uint8_t b, bool carry) {
    self->af.flags.n = 0;
    self->af.flags.c = ((self->af.hilo[HI] + b + carry) >> 8) & 1;
    self->af.flags.h = (self->af.hilo[HI] ^ b ^ (self->af.hilo[HI] + b + carry)) >> 4;
    self->af.flags.z = ((self->af.hilo[HI] + b + carry) & 0xff) == 0;
    return self->af.hilo[HI] + b + carry;
}

inline uint8_t sub8(sm83 *self, uint8_t b, bool carry) {
    self->af.flags.n = 1;
    self->af.flags.c = b + carry > self->af.hilo[HI];
    self->af.flags.h = (self->af.hilo[HI] ^ b ^ (self->af.hilo[HI] - b - carry)) >> 4;
    self->af.flags.z = ((self->af.hilo[HI] - b - carry) & 0xff) == 0;
    return self->af.hilo[HI] - b - carry;
}

inline uint8_t and8(sm83 *self, uint8_t b) {
    self->af.flags.n = 0;
    self->af.flags.c = 0;
    self->af.flags.h = 1;
    self->af.flags.z = (self->af.hilo[HI] & b) == 0;
    return self->af.hilo[HI] & b;
}

inline uint8_t or8(sm83 *self, uint8_t b) {
    self->af.flags.n = 0;
    self->af.flags.c = 0;
    self->af.flags.h = 0;
    self->af.flags.z = (self->af.hilo[HI] | b) == 0;
    return self->af.hilo[HI] | b;
}

inline uint8_t xor8(sm83 *self, uint8_t b) {
    self->af.flags.n = 0;
    self->af.flags.c = 0;
    self->af.flags.h = 0;
    self->af.flags.z = (self->af.hilo[HI] ^ b) == 0;
    return self->af.hilo[HI] ^ b;
}

inline void pop16(sm83 *self, uint16_t *val) {
    *val = mmu_read16(self->mmu, self->sp);
    self->sp += 2;
}

inline void call(sm83 *self) {
    mmu_write16(self->mmu, self->sp -= 2, self->pc + 2);
    self->pc = mmu_read16(self->mmu, self->pc);
}

inline uint8_t rst(sm83 *self, uint8_t val) {
    mmu_write16(self->mmu, self->sp -= 2, self->pc);
    self->pc = val;
    return 4;
}

inline uint8_t jrcond(sm83 *self, uint8_t cond) {
    if (cond) {
        self->pc += (int8_t)mmu_read8(self->mmu, self->pc) + 1;
        return 3;
    } else {
        ++self->pc;
        return 2;
    }
}

inline uint8_t jpcond(sm83 *self, uint8_t cond) {
    if (cond) {
        self->pc = mmu_read16(self->mmu, self->pc);
        return 4;
    } else {
        self->pc += 2;
        return 2;
    }
}

inline uint8_t retcond(sm83 *self, uint8_t cond) {
    if (cond) {
        pop16(self, &self->pc);
        return 5;
    } else {
        return 2;
    }
}

inline uint8_t callcond(sm83 *self, uint8_t cond) {
    if (cond) {
        call(self);
        return 6;
    } else {
        self->pc += 2;
        return 3;
    }
}

uint8_t sm83_step(sm83 *self) {
    uint8_t inst = mmu_read8(self->mmu, self->pc++);
    uint8_t tmp = 0, val; // this is needed for a few instructions
#ifdef DEBUG
    self->mmu->opcode = mmu_read8(self->mmu, self->pc);
#endif
    switch (inst) {
        case 0x00: // nop
            return 1;

        case 0x10: // stop
            return 1;

        // ime
        case 0xf3: self->ime = false; return 1;
        case 0xfb: self->ime = true; return 1;

        // jr
        case 0x18:
            self->pc += (int8_t)mmu_read8(self->mmu, self->pc) + 1;
            return 3;
        case 0x20: return jrcond(self, !self->af.flags.z);
        case 0x30: return jrcond(self, !self->af.flags.c);
        case 0x28: return jrcond(self, self->af.flags.z);
        case 0x38: return jrcond(self, self->af.flags.c);

        // jp instructions
        case 0xc3: // jp a16
            self->pc = mmu_read16(self->mmu, self->pc);
            return 4;
        case 0xe9: self->pc = self->hl.pair; return 1;
        case 0xc2: return jpcond(self, !self->af.flags.z);
        case 0xd2: return jpcond(self, !self->af.flags.c);
        case 0xca: return jpcond(self, self->af.flags.z);
        case 0xda: return jpcond(self, self->af.flags.c);

        // ret instructions
        case 0xc0: return retcond(self, !self->af.flags.z);
        case 0xd0: return retcond(self, !self->af.flags.c);
        case 0xc8: return retcond(self, self->af.flags.z);
        case 0xd8: return retcond(self, self->af.flags.c);
        case 0xc9: pop16(self, &self->pc); return 4;
        case 0xd9: // reti
            pop16(self, &self->pc);
            self->ime = true;
            return 4;

        // rst instructions
        case 0xc7: return rst(self, 0x00);
        case 0xd7: return rst(self, 0x10);
        case 0xe7: return rst(self, 0x20);
        case 0xf7: return rst(self, 0x30);
        case 0xcf: return rst(self, 0x08);
        case 0xdf: return rst(self, 0x18);
        case 0xef: return rst(self, 0x28);
        case 0xff: return rst(self, 0x38);

        // call instructions
        case 0xc4: return callcond(self, !self->af.flags.z);
        case 0xd4: return callcond(self, !self->af.flags.c);
        case 0xcc: return callcond(self, self->af.flags.z);
        case 0xdc: return callcond(self, self->af.flags.c);
        case 0xcd: call(self); return 6;

        // stack instructions, F's low 4 bits are ALWAYS ignored
        case 0xc1: pop16(self, &self->bc.pair); return 3;
        case 0xd1: pop16(self, &self->de.pair); return 3;
        case 0xe1: pop16(self, &self->hl.pair); return 3;
        case 0xf1:
            pop16(self, &self->af.pair);
            self->af.flags.lo = 0;
            return 3;
        case 0xc5: mmu_write16(self->mmu, self->sp -= 2, self->bc.pair); return 4;
        case 0xd5: mmu_write16(self->mmu, self->sp -= 2, self->de.pair); return 4;
        case 0xe5: mmu_write16(self->mmu, self->sp -= 2, self->hl.pair); return 4;
        case 0xf5: mmu_write16(self->mmu, self->sp -= 2, self->af.pair & 0xfff0); return 4;

        // rotate instructions
        case 0x07: // rlca
            self->af.flags.h = 0;
            self->af.flags.n = 0;
            self->af.flags.z = 0;
            self->af.flags.c = self->af.hilo[HI] >> 7;
            self->af.hilo[HI] = (self->af.hilo[HI] << 1) | self->af.flags.c;
            return 1;
        case 0x17: // rla
            self->af.flags.h = 0;
            self->af.flags.n = 0;
            self->af.flags.z = 0;
            tmp = self->af.flags.c;
            self->af.flags.c = self->af.hilo[HI] >> 7;
            self->af.hilo[HI] = (self->af.hilo[HI] << 1) | tmp;
            return 1;
        case 0x0f: // rrca
            self->af.flags.h = 0;
            self->af.flags.n = 0;
            self->af.flags.z = 0;
            self->af.flags.c = self->af.hilo[HI] & 1;
            self->af.hilo[HI] = (self->af.hilo[HI] >> 1) | (self->af.flags.c << 7);
            return 1;
        case 0x1f: // rra
            self->af.flags.h = 0;
            self->af.flags.n = 0;
            self->af.flags.z = 0;
            tmp = self->af.flags.c;
            self->af.flags.c = self->af.hilo[HI] & 1;
            self->af.hilo[HI] = (self->af.hilo[HI] >> 1) | (tmp << 7);
            return 1;

        // flag instructions
        case 0x37: // scf
            self->af.flags.n = 0;
            self->af.flags.h = 0;
            self->af.flags.c = 1;
            return 1;
        case 0x2f: // cpl
            self->af.hilo[HI] = ~self->af.hilo[HI];
            self->af.flags.n = 1;
            self->af.flags.h = 1;
            return 1;
        case 0x3f: // ccf
            self->af.flags.n = 0;
            self->af.flags.h = 0;
            self->af.flags.c = !self->af.flags.c;
            return 1;

        // daa (the final boss of instructions)
        case 0x27:
            if (!self->af.flags.n) {
                if (self->af.flags.c || self->af.hilo[HI] > 0x99) {
                    self->af.hilo[HI] += 0x60;
                    self->af.flags.c = 1;
                }
                if (self->af.flags.h || (self->af.hilo[HI] & 0x0f) > 0x09) {
                    self->af.hilo[HI] += 0x6;
                }
            } else {
                if (self->af.flags.c)
                    self->af.hilo[HI] -= 0x60;
                if (self->af.flags.h)
                    self->af.hilo[HI] -= 0x6;
            }
            self->af.flags.z = self->af.hilo[HI] == 0;
            self->af.flags.h = 0;
            return 1;

        // ld xx, n16
        case 0x01:
            self->bc.pair = mmu_read16(self->mmu, self->pc);
            self->pc += 2;
            return 3;
        case 0x11:
            self->de.pair = mmu_read16(self->mmu, self->pc);
            self->pc += 2;
            return 3;
        case 0x21:
            self->hl.pair = mmu_read16(self->mmu, self->pc);
            self->pc += 2;
            return 3;
        case 0x31:
            self->sp = mmu_read16(self->mmu, self->pc);
            self->pc += 2;
            return 3;

        // ld [xx], a
        case 0x02: mmu_write8(self->mmu, self->bc.pair, self->af.hilo[HI]); return 2;
        case 0x12: mmu_write8(self->mmu, self->de.pair, self->af.hilo[HI]); return 2;
        case 0x22: mmu_write8(self->mmu, self->hl.pair++, self->af.hilo[HI]); return 2;
        case 0x32: mmu_write8(self->mmu, self->hl.pair--, self->af.hilo[HI]); return 2;

        // inc xx
        case 0x03: ++self->bc.pair; return 2;
        case 0x13: ++self->de.pair; return 2;
        case 0x23: ++self->hl.pair; return 2;
        case 0x33: ++self->sp; return 2;

        // inc x
        case 0x04:
            ++self->bc.hilo[HI];
            self->af.flags.z = self->bc.hilo[HI] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->bc.hilo[HI] & 0xf) == 0;
            return 1;
        case 0x14:
            ++self->de.hilo[HI];
            self->af.flags.z = self->de.hilo[HI] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->de.hilo[HI] & 0xf) == 0;
            return 1;
        case 0x24:
            ++self->hl.hilo[HI];
            self->af.flags.z = self->hl.hilo[HI] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->hl.hilo[HI] & 0xf) == 0;
            return 1;
        case 0x34:
            tmp = mmu_read8(self->mmu, self->hl.pair) + 1;
            mmu_write8(self->mmu, self->hl.pair, tmp);
            self->af.flags.z = tmp == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (tmp & 0xf) == 0;
            return 3;

        // dec x
        case 0x05:
            --self->bc.hilo[HI];
            self->af.flags.z = self->bc.hilo[HI] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = (self->bc.hilo[HI] & 0xf) == 0xf;
            return 1;
        case 0x15:
            --self->de.hilo[HI];
            self->af.flags.z = self->de.hilo[HI] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = (self->de.hilo[HI] & 0xf) == 0xf;
            return 1;
        case 0x25:
            --self->hl.hilo[HI];
            self->af.flags.z = self->hl.hilo[HI] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = (self->hl.hilo[HI] & 0xf) == 0xf;
            return 1;
        case 0x35:
            mmu_write8(self->mmu, self->hl.pair, tmp = mmu_read8(self->mmu, self->hl.pair) - 1);
            self->af.flags.z = tmp == 0;
            self->af.flags.n = 1;
            self->af.flags.h = (tmp & 0xf) == 0xf;
            return 3;

        // ld x, n8
        case 0x06: self->bc.hilo[HI] = mmu_read8(self->mmu, self->pc++); return 2;
        case 0x16: self->de.hilo[HI] = mmu_read8(self->mmu, self->pc++); return 2;
        case 0x26: self->hl.hilo[HI] = mmu_read8(self->mmu, self->pc++); return 2;
        case 0x36: mmu_write8(self->mmu, self->hl.pair, mmu_read8(self->mmu, self->pc++)); return 3;

        // add hl, xx
        case 0x09:
            self->af.flags.n = 0;
            self->af.flags.h = (((self->hl.pair & 0xfff) + (self->bc.pair & 0xfff)) >> 12) & 1;
            self->af.flags.c = ((self->hl.pair + self->bc.pair) >> 16) & 1;
            self->hl.pair += self->bc.pair;
            return 2;
        case 0x19:
            self->af.flags.n = 0;
            self->af.flags.h = (((self->hl.pair & 0xfff) + (self->de.pair & 0xfff)) >> 12) & 1;
            self->af.flags.c = ((self->hl.pair + self->de.pair) >> 16) & 1;
            self->hl.pair += self->de.pair;
            return 2;
        case 0x29:
            self->af.flags.n = 0;
            self->af.flags.h = (((self->hl.pair & 0xfff) + (self->hl.pair & 0xfff)) >> 12) & 1;
            self->af.flags.c = ((self->hl.pair + self->hl.pair) >> 16) & 1;
            self->hl.pair += self->hl.pair;
            return 2;
        case 0x39:
            self->af.flags.n = 0;
            self->af.flags.h = (((self->hl.pair & 0xfff) + (self->sp & 0xfff)) >> 12) & 1;
            self->af.flags.c = ((self->hl.pair + self->sp) >> 16) & 1;
            self->hl.pair += self->sp;
            return 2;

        // ld a, [xx]
        case 0x0a: self->af.hilo[HI] = mmu_read8(self->mmu, self->bc.pair); return 2;
        case 0x1a: self->af.hilo[HI] = mmu_read8(self->mmu, self->de.pair); return 2;
        case 0x2a: self->af.hilo[HI] = mmu_read8(self->mmu, self->hl.pair++); return 2;
        case 0x3a: self->af.hilo[HI] = mmu_read8(self->mmu, self->hl.pair--); return 2;

        // dec xx
        case 0x0b: --self->bc.pair; return 2;
        case 0x1b: --self->de.pair; return 2;
        case 0x2b: --self->hl.pair; return 2;
        case 0x3b: --self->sp; return 2;

        // inc x
        case 0x0c:
            ++self->bc.hilo[LO];
            self->af.flags.z = self->bc.hilo[LO] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->bc.hilo[LO] & 0xf) == 0;
            return 1;
        case 0x1c:
            ++self->de.hilo[LO];
            self->af.flags.z = self->de.hilo[LO] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->de.hilo[LO] & 0xf) == 0;
            return 1;
        case 0x2c:
            ++self->hl.hilo[LO];
            self->af.flags.z = self->hl.hilo[LO] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->hl.hilo[LO] & 0xf) == 0;
            return 1;
        case 0x3c:
            ++self->af.hilo[HI];
            self->af.flags.z = self->af.hilo[HI] == 0;
            self->af.flags.n = 0;
            self->af.flags.h = (self->af.hilo[HI] & 0xf) == 0;
            return 1;

        // dec x
        case 0x0d:
            --self->bc.hilo[LO];
            self->af.flags.z = self->bc.hilo[LO] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = ((self->bc.hilo[LO] & 0xf) == 0xf);
            return 1;
        case 0x1d:
            --self->de.hilo[LO];
            self->af.flags.z = self->de.hilo[LO] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = ((self->de.hilo[LO] & 0xf) == 0xf);
            return 1;
        case 0x2d:
            --self->hl.hilo[LO];
            self->af.flags.z = self->hl.hilo[LO] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = ((self->hl.hilo[LO] & 0xf) == 0xf);
            return 1;
        case 0x3d:
            --self->af.hilo[HI];
            self->af.flags.z = self->af.hilo[HI] == 0;
            self->af.flags.n = 1;
            self->af.flags.h = ((self->af.hilo[HI] & 0xf) == 0xf);
            return 1;

        // ld x, n8
        case 0x0e: self->bc.hilo[LO] = mmu_read8(self->mmu, self->pc++); return 2;
        case 0x1e: self->de.hilo[LO] = mmu_read8(self->mmu, self->pc++); return 2;
        case 0x2e: self->hl.hilo[LO] = mmu_read8(self->mmu, self->pc++); return 2;
        case 0x3e: self->af.hilo[HI] = mmu_read8(self->mmu, self->pc++); return 2;

        // ld x, x
        case 0x40: self->bc.hilo[HI] = self->bc.hilo[HI]; return 1;
        case 0x41: self->bc.hilo[HI] = self->bc.hilo[LO]; return 1;
        case 0x42: self->bc.hilo[HI] = self->de.hilo[HI]; return 1;
        case 0x43: self->bc.hilo[HI] = self->de.hilo[LO]; return 1;
        case 0x44: self->bc.hilo[HI] = self->hl.hilo[HI]; return 1;
        case 0x45: self->bc.hilo[HI] = self->hl.hilo[LO]; return 1;
        case 0x46: self->bc.hilo[HI] = mmu_read8(self->mmu, self->hl.pair); return 2;
        case 0x47: self->bc.hilo[HI] = self->af.hilo[HI]; return 1;
        case 0x48: self->bc.hilo[LO] = self->bc.hilo[HI]; return 1;
        case 0x49: self->bc.hilo[LO] = self->bc.hilo[LO]; return 1;
        case 0x4a: self->bc.hilo[LO] = self->de.hilo[HI]; return 1;
        case 0x4b: self->bc.hilo[LO] = self->de.hilo[LO]; return 1;
        case 0x4c: self->bc.hilo[LO] = self->hl.hilo[HI]; return 1;
        case 0x4d: self->bc.hilo[LO] = self->hl.hilo[LO]; return 1;
        case 0x4e: self->bc.hilo[LO] = mmu_read8(self->mmu, self->hl.pair); return 2;
        case 0x4f: self->bc.hilo[LO] = self->af.hilo[HI]; return 1;
        case 0x50: self->de.hilo[HI] = self->bc.hilo[HI]; return 1;
        case 0x51: self->de.hilo[HI] = self->bc.hilo[LO]; return 1;
        case 0x52: self->de.hilo[HI] = self->de.hilo[HI]; return 1;
        case 0x53: self->de.hilo[HI] = self->de.hilo[LO]; return 1;
        case 0x54: self->de.hilo[HI] = self->hl.hilo[HI]; return 1;
        case 0x55: self->de.hilo[HI] = self->hl.hilo[LO]; return 1;
        case 0x56: self->de.hilo[HI] = mmu_read8(self->mmu, self->hl.pair); return 2;
        case 0x57: self->de.hilo[HI] = self->af.hilo[HI]; return 1;
        case 0x58: self->de.hilo[LO] = self->bc.hilo[HI]; return 1;
        case 0x59: self->de.hilo[LO] = self->bc.hilo[LO]; return 1;
        case 0x5a: self->de.hilo[LO] = self->de.hilo[HI]; return 1;
        case 0x5b: self->de.hilo[LO] = self->de.hilo[LO]; return 1;
        case 0x5c: self->de.hilo[LO] = self->hl.hilo[HI]; return 1;
        case 0x5d: self->de.hilo[LO] = self->hl.hilo[LO]; return 1;
        case 0x5e: self->de.hilo[LO] = mmu_read8(self->mmu, self->hl.pair); return 2;
        case 0x5f: self->de.hilo[LO] = self->af.hilo[HI]; return 1;
        case 0x60: self->hl.hilo[HI] = self->bc.hilo[HI]; return 1;
        case 0x61: self->hl.hilo[HI] = self->bc.hilo[LO]; return 1;
        case 0x62: self->hl.hilo[HI] = self->de.hilo[HI]; return 1;
        case 0x63: self->hl.hilo[HI] = self->de.hilo[LO]; return 1;
        case 0x64: self->hl.hilo[HI] = self->hl.hilo[HI]; return 1;
        case 0x65: self->hl.hilo[HI] = self->hl.hilo[LO]; return 1;
        case 0x66: self->hl.hilo[HI] = mmu_read8(self->mmu, self->hl.pair); return 2;
        case 0x67: self->hl.hilo[HI] = self->af.hilo[HI]; return 1;
        case 0x68: self->hl.hilo[LO] = self->bc.hilo[HI]; return 1;
        case 0x69: self->hl.hilo[LO] = self->bc.hilo[LO]; return 1;
        case 0x6a: self->hl.hilo[LO] = self->de.hilo[HI]; return 1;
        case 0x6b: self->hl.hilo[LO] = self->de.hilo[LO]; return 1;
        case 0x6c: self->hl.hilo[LO] = self->hl.hilo[HI]; return 1;
        case 0x6d: self->hl.hilo[LO] = self->hl.hilo[LO]; return 1;
        case 0x6e: self->hl.hilo[LO] = mmu_read8(self->mmu, self->hl.pair); return 2;
        case 0x6f: self->hl.hilo[LO] = self->af.hilo[HI]; return 1;
        case 0x70: mmu_write8(self->mmu, self->hl.pair, self->bc.hilo[HI]); return 2;
        case 0x71: mmu_write8(self->mmu, self->hl.pair, self->bc.hilo[LO]); return 2;
        case 0x72: mmu_write8(self->mmu, self->hl.pair, self->de.hilo[HI]); return 2;
        case 0x73: mmu_write8(self->mmu, self->hl.pair, self->de.hilo[LO]); return 2;
        case 0x74: mmu_write8(self->mmu, self->hl.pair, self->hl.hilo[HI]); return 2;
        case 0x75: mmu_write8(self->mmu, self->hl.pair, self->hl.hilo[LO]); return 2;
        case 0x76: self->halt = true; return 1;
        case 0x77: mmu_write8(self->mmu, self->hl.pair, self->af.hilo[HI]); return 2;
        case 0x78: self->af.hilo[HI] = self->bc.hilo[HI]; return 1;
        case 0x79: self->af.hilo[HI] = self->bc.hilo[LO]; return 1;
        case 0x7a: self->af.hilo[HI] = self->de.hilo[HI]; return 1;
        case 0x7b: self->af.hilo[HI] = self->de.hilo[LO]; return 1;
        case 0x7c: self->af.hilo[HI] = self->hl.hilo[HI]; return 1;
        case 0x7d: self->af.hilo[HI] = self->hl.hilo[LO]; return 1;
        case 0x7e: self->af.hilo[HI] = mmu_read8(self->mmu, self->hl.pair); return 2;
        case 0x7f: self->af.hilo[HI] = self->af.hilo[HI]; return 1;

        // wierd ld instructions
        case 0xe0:
            mmu_write8(self->mmu, mmu_read8(self->mmu, self->pc++) + 0xff00, self->af.hilo[HI]);
            return 3;
        case 0xf0:
            self->af.hilo[HI] = mmu_read8(self->mmu, mmu_read8(self->mmu, self->pc++) + 0xff00);
            return 3;
        case 0xe2:
            mmu_write8(self->mmu, self->bc.hilo[LO] + 0xff00, self->af.hilo[HI]);
            return 2;
        case 0xf2:
            self->af.hilo[HI] = mmu_read8(self->mmu, self->bc.hilo[LO] + 0xff00);
            return 2;
        case 0xea:
            mmu_write8(self->mmu, mmu_read16(self->mmu, self->pc), self->af.hilo[HI]);
            self->pc += 2;
            return 4;
        case 0xfa:
            self->af.hilo[HI] = mmu_read8(self->mmu, mmu_read16(self->mmu, self->pc));
            self->pc += 2;
            return 4;
        case 0xf8:
            tmp = mmu_read8(self->mmu, self->pc++);
            self->af.flags.n = 0;
            self->af.flags.z = 0;
            self->af.flags.h = (self->sp ^ (int8_t)tmp ^ (self->sp + (int8_t)tmp)) >> 4;
            self->af.flags.c = (self->sp ^ (int8_t)tmp ^ (self->sp + (int8_t)tmp)) >> 8;
            self->hl.pair = self->sp + (int8_t)tmp;
            return 3;
        case 0xf9:
            self->sp = self->hl.pair;
            return 2;
        case 0x08:
            mmu_write16(self->mmu, mmu_read16(self->mmu, self->pc), self->sp);
            self->pc += 2;
            return 5;

        // logical instructions
        // this wierd add sp, e8 thing
        case 0xe8:
            tmp = mmu_read8(self->mmu, self->pc++);
            self->af.flags.n = 0;
            self->af.flags.z = 0;
            self->af.flags.h = (self->sp ^ (int8_t)tmp ^ (self->sp + (int8_t)tmp)) >> 4;
            self->af.flags.c = (self->sp ^ (int8_t)tmp ^ (self->sp + (int8_t)tmp)) >> 8;
            self->sp += (int8_t)tmp;
            return 4;
        // add
        case 0x80: self->af.hilo[HI] = add8(self, self->bc.hilo[HI], 0); return 1;
        case 0x81: self->af.hilo[HI] = add8(self, self->bc.hilo[LO], 0); return 1;
        case 0x82: self->af.hilo[HI] = add8(self, self->de.hilo[HI], 0); return 1;
        case 0x83: self->af.hilo[HI] = add8(self, self->de.hilo[LO], 0); return 1;
        case 0x84: self->af.hilo[HI] = add8(self, self->hl.hilo[HI], 0); return 1;
        case 0x85: self->af.hilo[HI] = add8(self, self->hl.hilo[LO], 0); return 1;
        case 0x86: self->af.hilo[HI] = add8(self, mmu_read8(self->mmu, self->hl.pair), 0); return 2;
        case 0x87: self->af.hilo[HI] = add8(self, self->af.hilo[HI], 0); return 1;
        // adc
        case 0x88: self->af.hilo[HI] = add8(self, self->bc.hilo[HI], self->af.flags.c); return 1;
        case 0x89: self->af.hilo[HI] = add8(self, self->bc.hilo[LO], self->af.flags.c); return 1;
        case 0x8a: self->af.hilo[HI] = add8(self, self->de.hilo[HI], self->af.flags.c); return 1;
        case 0x8b: self->af.hilo[HI] = add8(self, self->de.hilo[LO], self->af.flags.c); return 1;
        case 0x8c: self->af.hilo[HI] = add8(self, self->hl.hilo[HI], self->af.flags.c); return 1;
        case 0x8d: self->af.hilo[HI] = add8(self, self->hl.hilo[LO], self->af.flags.c); return 1;
        case 0x8e: self->af.hilo[HI] = add8(self, mmu_read8(self->mmu, self->hl.pair), self->af.flags.c); return 2;
        case 0x8f: self->af.hilo[HI] = add8(self, self->af.hilo[HI], self->af.flags.c); return 1;
        // sub
        case 0x90: self->af.hilo[HI] = sub8(self, self->bc.hilo[HI], 0); return 1;
        case 0x91: self->af.hilo[HI] = sub8(self, self->bc.hilo[LO], 0); return 1;
        case 0x92: self->af.hilo[HI] = sub8(self, self->de.hilo[HI], 0); return 1;
        case 0x93: self->af.hilo[HI] = sub8(self, self->de.hilo[LO], 0); return 1;
        case 0x94: self->af.hilo[HI] = sub8(self, self->hl.hilo[HI], 0); return 1;
        case 0x95: self->af.hilo[HI] = sub8(self, self->hl.hilo[LO], 0); return 1;
        case 0x96: self->af.hilo[HI] = sub8(self, mmu_read8(self->mmu, self->hl.pair), 0); return 2;
        case 0x97: self->af.hilo[HI] = sub8(self, self->af.hilo[HI], 0); return 1;
        // sbc
        case 0x98: self->af.hilo[HI] = sub8(self, self->bc.hilo[HI], self->af.flags.c); return 1;
        case 0x99: self->af.hilo[HI] = sub8(self, self->bc.hilo[LO], self->af.flags.c); return 1;
        case 0x9a: self->af.hilo[HI] = sub8(self, self->de.hilo[HI], self->af.flags.c); return 1;
        case 0x9b: self->af.hilo[HI] = sub8(self, self->de.hilo[LO], self->af.flags.c); return 1;
        case 0x9c: self->af.hilo[HI] = sub8(self, self->hl.hilo[HI], self->af.flags.c); return 1;
        case 0x9d: self->af.hilo[HI] = sub8(self, self->hl.hilo[LO], self->af.flags.c); return 1;
        case 0x9e: self->af.hilo[HI] = sub8(self, mmu_read8(self->mmu, self->hl.pair), self->af.flags.c); return 2;
        case 0x9f: self->af.hilo[HI] = sub8(self, self->af.hilo[HI], self->af.flags.c); return 1;
        // and
        case 0xa0: self->af.hilo[HI] = and8(self, self->bc.hilo[HI]); return 1;
        case 0xa1: self->af.hilo[HI] = and8(self, self->bc.hilo[LO]); return 1;
        case 0xa2: self->af.hilo[HI] = and8(self, self->de.hilo[HI]); return 1;
        case 0xa3: self->af.hilo[HI] = and8(self, self->de.hilo[LO]); return 1;
        case 0xa4: self->af.hilo[HI] = and8(self, self->hl.hilo[HI]); return 1;
        case 0xa5: self->af.hilo[HI] = and8(self, self->hl.hilo[LO]); return 1;
        case 0xa6: self->af.hilo[HI] = and8(self, mmu_read8(self->mmu, self->hl.pair)); return 2;
        case 0xa7: self->af.hilo[HI] = and8(self, self->af.hilo[HI]); return 1;
        // xor
        case 0xa8: self->af.hilo[HI] = xor8(self, self->bc.hilo[HI]); return 1;
        case 0xa9: self->af.hilo[HI] = xor8(self, self->bc.hilo[LO]); return 1;
        case 0xaa: self->af.hilo[HI] = xor8(self, self->de.hilo[HI]); return 1;
        case 0xab: self->af.hilo[HI] = xor8(self, self->de.hilo[LO]); return 1;
        case 0xac: self->af.hilo[HI] = xor8(self, self->hl.hilo[HI]); return 1;
        case 0xad: self->af.hilo[HI] = xor8(self, self->hl.hilo[LO]); return 1;
        case 0xae: self->af.hilo[HI] = xor8(self, mmu_read8(self->mmu, self->hl.pair)); return 2;
        case 0xaf: self->af.hilo[HI] = xor8(self, self->af.hilo[HI]); return 1;
        // or
        case 0xb0: self->af.hilo[HI] = or8(self, self->bc.hilo[HI]); return 1;
        case 0xb1: self->af.hilo[HI] = or8(self, self->bc.hilo[LO]); return 1;
        case 0xb2: self->af.hilo[HI] = or8(self, self->de.hilo[HI]); return 1;
        case 0xb3: self->af.hilo[HI] = or8(self, self->de.hilo[LO]); return 1;
        case 0xb4: self->af.hilo[HI] = or8(self, self->hl.hilo[HI]); return 1;
        case 0xb5: self->af.hilo[HI] = or8(self, self->hl.hilo[LO]); return 1;
        case 0xb6: self->af.hilo[HI] = or8(self, mmu_read8(self->mmu, self->hl.pair)); return 2;
        case 0xb7: self->af.hilo[HI] = or8(self, self->af.hilo[HI]); return 1;
        // cp
        case 0xb8: sub8(self, self->bc.hilo[HI], 0); return 1;
        case 0xb9: sub8(self, self->bc.hilo[LO], 0); return 1;
        case 0xba: sub8(self, self->de.hilo[HI], 0); return 1;
        case 0xbb: sub8(self, self->de.hilo[LO], 0); return 1;
        case 0xbc: sub8(self, self->hl.hilo[HI], 0); return 1;
        case 0xbd: sub8(self, self->hl.hilo[LO], 0); return 1;
        case 0xbe: sub8(self, mmu_read8(self->mmu, self->hl.pair), 0); return 2;
        case 0xbf: sub8(self, self->af.hilo[HI], 0); return 1;

        // logic n8 instructions
        case 0xc6: self->af.hilo[HI] = add8(self, mmu_read8(self->mmu, self->pc++), 0); return 2;
        case 0xce: self->af.hilo[HI] = add8(self, mmu_read8(self->mmu, self->pc++), self->af.flags.c); return 2;
        case 0xd6: self->af.hilo[HI] = sub8(self, mmu_read8(self->mmu, self->pc++), 0); return 2;
        case 0xde: self->af.hilo[HI] = sub8(self, mmu_read8(self->mmu, self->pc++), self->af.flags.c); return 2;
        case 0xe6: self->af.hilo[HI] = and8(self, mmu_read8(self->mmu, self->pc++)); return 2;
        case 0xee: self->af.hilo[HI] = xor8(self, mmu_read8(self->mmu, self->pc++)); return 2;
        case 0xf6: self->af.hilo[HI] = or8(self, mmu_read8(self->mmu, self->pc++)); return 2;
        case 0xfe: sub8(self, mmu_read8(self->mmu, self->pc++), 0); return 2;

        //    0xcb instruction encodings
        //    reg
        //    000 b
        //    001 c
        //    010 d
        //    011 e
        //    100 h
        //    101 l
        //    110 [hl]
        //    111 a
        //
        //    top 2 bits = 00
        //    00000 reg rlc
        //    00001 reg rrc
        //    00010 reg rl
        //    00011 reg rr
        //    00100 reg sla
        //    00101 reg sra
        //    00110 reg swap
        //    00111 reg srl
        //
        //    bitnum is a 3 bit literal
        //    01 bitnum reg bit
        //    10 bitnum reg res
        //    11 bitnum reg set
        case 0xcb:
            inst = mmu_read8(self->mmu, self->pc++);
            switch (inst & 7) {
                case 0: val = self->bc.hilo[HI]; break;
                case 1: val = self->bc.hilo[LO]; break;
                case 2: val = self->de.hilo[HI]; break;
                case 3: val = self->de.hilo[LO]; break;
                case 4: val = self->hl.hilo[HI]; break;
                case 5: val = self->hl.hilo[LO]; break;
                case 6: val = mmu_read8(self->mmu, self->hl.pair); break;
                case 7: val = self->af.hilo[HI]; break;
            }
            switch (inst & 0xc0) {
                case 0x40: // bit
                    self->af.flags.z = !((val >> ((inst >> 3) & 0x7)) & 1);
                    self->af.flags.n = 0;
                    self->af.flags.h = 1;
                    return 2; // return because bit does not write back to the register
                case 0x80: // res
                    val &= (0xfe << ((inst >> 3) & 0x7)) | (0xff >> (8 - ((inst >> 3) & 0x7)));
                    break;
                case 0xc0: // set
                    val |= (0x1 << ((inst >> 3) & 0x7));
                    break;
                case 0x00:
                    self->af.flags.n = 0;
                    self->af.flags.h = 0;
                    switch (inst & 0x38) {
                        case 0x00: // rlc
                            self->af.flags.c = val >> 7;
                            val = (val << 1) | self->af.flags.c;
                            break;
                        case 0x08: // rrc
                            self->af.flags.c = val & 1;
                            val = (val >> 1) | (self->af.flags.c << 7);
                            break;
                        case 0x10: // rl
                            tmp = self->af.flags.c;
                            self->af.flags.c = val >> 7;
                            val = (val << 1) | tmp;
                            break;
                        case 0x18: // rr
                            tmp = self->af.flags.c;
                            self->af.flags.c = val & 1;
                            val = (val >> 1) | (tmp << 7);
                            break;
                        case 0x20: // sla
                            self->af.flags.c = val >> 7;
                            val <<= 1;
                            break;
                        case 0x28: // sra
                            self->af.flags.c = val & 1;
                            val = (val >> 1) | (val & 0x80);
                            break;
                        case 0x30: // swap
                            self->af.flags.c = 0;
                            val = (val >> 4) | (val << 4);
                            break;
                        case 0x38: // srl
                            self->af.flags.c = val & 1;
                            val >>= 1;
                            break;
                    }
                    self->af.flags.z = val == 0;
                    break;
            }
            // write back to register
            switch (inst & 7) {
                case 0: self->bc.hilo[HI] = val; break;
                case 1: self->bc.hilo[LO] = val; break;
                case 2: self->de.hilo[HI] = val; break;
                case 3: self->de.hilo[LO] = val; break;
                case 4: self->hl.hilo[HI] = val; break;
                case 5: self->hl.hilo[LO] = val; break;
                case 6: mmu_write8(self->mmu, self->hl.pair, val); break;
                case 7: self->af.hilo[HI] = val; break;
            }
            return 2;

        default:
#ifdef DEBUG
            fprintf(stderr, "warning: tried to execute unrecognised opcode \"0x%02x\"\n", mmu_read8(self->mmu, self->pc));
#endif
            break;
    }
    return 1;
}
