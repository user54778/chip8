#include "chip8.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: ./main <file>\n");
    return EXIT_FAILURE;
  }

  Chip8 *chip8 = CreateChip8();
  if (chip8 == NULL) {
    fprintf(stderr, "failed to create Chip8 object\n");
    return EXIT_FAILURE;
  }

  if (!LoadRom(chip8, argv[1])) {
    fprintf(stderr, "failed to load rom: %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  // TODO: Initialize raylib here!
  // Initialize CPU here!
  return 0;
}
