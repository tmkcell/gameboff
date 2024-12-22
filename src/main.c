#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
  const char *help = "gameboff [options] rom...\n"
                     "Options:\n"
                     "    -h     Returns help menu\n"
                     "    -v     Returns the program version\n";
  for (int i = 1; i < argc - 1; ++i) {
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
          return(1);
      }
    } else {
      fprintf(stderr, "Unrecognised option \"%s\"\n%s", argv[i], help);
      return(1);
    }
  }

  if (access(argv[argc-1], F_OK | R_OK) == -1) { // we need read permissions if a file exists
    fprintf(stderr, "Unable to read file \"%s\"", argv[argc-1]);
    return 1;
  }

  return 0;
}
