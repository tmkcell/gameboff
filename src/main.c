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
            if (argc == 2)
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
    uint32_t programSize = ftell(rom_ptr);
    rewind(rom_ptr);

    sm83 *cpu = malloc(sizeof(sm83));
    sm83_init(cpu, 0x10000, bootrom_ptr, rom_ptr);

    sm83_deinit(cpu);
    fclose(rom_ptr);
    return 0;
}
