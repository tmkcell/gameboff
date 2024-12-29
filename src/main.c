#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cpu.h"

int main(int argc, char **argv) {
    const char *help = "gameboff [options] rom...\n"
                       "Options:\n"
                       "    -b [bootrom] Use bootrom 'bootrom'\n"
                       "    -h           Returns help menu\n"
                       "    -v           Returns the program version\n";
    FILE *bootrom_ptr = NULL, *rom_ptr = NULL;
    if (argc == 1) {
        fprintf(stderr, "No ROM path specified\n%s", help);
        return 1;
    }
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'b':
                    bootrom_ptr = fopen(argv[++i], "rb");
                    break;
                case 'v':
                    fprintf(stderr, "%s", PKG_VER);
                    return 0;
                case 'h':
                    fprintf(stderr, "%s", help);
                    return 0;
                default:
                    fprintf(stderr, "Unrecognised option \"%s\"\n%s", argv[i], help);
                    return 1;
            }
        } else {
            if (i == argc - 1)
                break;
            fprintf(stderr, "Unrecognised option \"%s\"\n%s", argv[i], help);
            return 1;
        }
    }

    // we need read permissions if a file exists
    if (access(argv[argc - 1], F_OK | R_OK) == -1) {
        fprintf(stderr, "Unable to read file \"%s\"", argv[argc - 1]);
        return 1;
    }

    // get program size
    rom_ptr = fopen(argv[argc - 1], "rb");
    fseek(rom_ptr, 0, SEEK_END);
    uint32_t program_size = ftell(rom_ptr);
    rewind(rom_ptr);

    sm83 cpu;
    sm83_init(&cpu, program_size, bootrom_ptr, rom_ptr);

#ifdef DEBUG
    FILE *log = fopen("log.txt", "w+"), *dump = fopen("dump.bin", "w+");
#endif
    do {
#ifdef DEBUG
        if (cpu.mmu->mem[0xdffd] == 47)
            break;
        fprintf(log, "A:%02x F:%02x B:%02x C:%02x D:%02x E:%02x H:%02x L:%02x SP:%04x PC:%04x PCMEM:%02x,%02x,%02x,%02x\n",
            cpu.af.hilo[HI], cpu.af.hilo[LO], cpu.bc.hilo[HI], cpu.bc.hilo[LO], cpu.de.hilo[HI], cpu.de.hilo[LO],
            cpu.hl.hilo[HI], cpu.hl.hilo[LO], cpu.sp, cpu.pc, mmu_read8(cpu.mmu, cpu.pc), mmu_read8(cpu.mmu, cpu.pc + 1),
            mmu_read8(cpu.mmu, cpu.pc + 2), mmu_read8(cpu.mmu, cpu.pc + 3));
        fwrite(cpu.mmu->mem, 1, 0x10000, dump);
        rewind(dump);
#endif
        sm83_step(&cpu);
    } while (!cpu.halt);

    sm83_deinit(&cpu);
    fclose(rom_ptr);
    fclose(bootrom_ptr);
#ifdef DEBUG
    fclose(log);
    fclose(dump);
#endif
    return 0;
}
