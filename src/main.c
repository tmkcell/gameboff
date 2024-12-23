#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *help = "gameboff [options] rom...\n"
                       "Options:\n"
                       "    -h     Returns help menu\n"
                       "    -v     Returns the program version\n";
    if (argc == 1) {
        fprintf(stderr, "No ROM path specified\n%s", help);
        return 1;
    }
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'v':
                fprintf(stderr, "%s", PKG_VER);
                return 1;
            case 'h':
                fprintf(stderr, "%s", help);
                return 1;
            default:
                fprintf(stderr, "Unrecognised option \"%s\"\n%s", argv[i], help);
                return (1);
            }
        } else {
            if (argc == 2)
                break;
            fprintf(stderr, "Unrecognised option \"%s\"\n%s", argv[i], help);
            return (1);
        }
    }

    // we need read permissions if a file exists
    if (access(argv[argc - 1], F_OK | R_OK) == -1) {
        fprintf(stderr, "Unable to read file \"%s\"", argv[argc - 1]);
        return 1;
    }

    FILE *filePtr = NULL;
    filePtr = fopen(argv[argc - 1], "rb");
    // Puts the file position pointer at end of the file
    fseek(filePtr, 0, SEEK_END);
    // Gives the position of the file position pointer
    uint32_t programSize = ftell(filePtr);
    // Puts the file position pointer to the beginning of the file
    rewind(filePtr);

    // THE REST OF THE CODE GOES HERE

    fclose(filePtr);
    filePtr = NULL;
    return 0;
}
