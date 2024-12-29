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
    FILE *bootrom_f = NULL, *rom_f = NULL;
    uint8_t *bootrom = NULL, *rom = NULL;
    if (argc == 1) {
        fprintf(stderr, "No ROM path specified\n%s", help);
        return 1;
    }
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'b':
                    if (!bootrom) {
                        if (access(argv[++i], F_OK | R_OK) == -1) {
                            fprintf(stderr, "Unable to read bootrom \"%s\"", argv[i]);
                            return 1;
                        }
                        bootrom_f = fopen(argv[i], "rb");
                        bootrom = malloc(0x100);
                        fread(bootrom, 1, 0x100, bootrom_f);
                    }
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
        fprintf(stderr, "Unable to read rom \"%s\"", argv[argc - 1]);
        return 1;
    }

    // load rom
    rom_f = fopen(argv[argc - 1], "rb");
    fseek(rom_f, 0, SEEK_END);
    uint32_t rom_size = ftell(rom_f);
    rewind(rom_f);
    rom = malloc(rom_size);
    fread(rom, 1, rom_size, rom_f);

    sm83 cpu;
    sm83_init(&cpu, bootrom, rom);

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
    free(rom);
    fclose(rom_f);
    if (bootrom) {
        free(bootrom);
        fclose(bootrom_f);
    }
#ifdef DEBUG
    fclose(log);
    fclose(dump);
#endif
    return 0;
}
