#include "chip8.h"
#include "raylib.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "usage: ./main <rom> <wav>\n");
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

  const int screenWidth = 64;
  const int screenHeight = 32;
  const int scale = 10;
  /*
  for (unsigned int i = 0; i < sizeof(chip8->memory); i++) {
    if (chip8->memory[i] == 0 || i <= 80) {
      continue;
    }
    printf("%x\n", chip8->memory[i]);
  }
  */

  // Chip8 window
  InitWindow(screenWidth * scale, screenHeight * scale, "CHIP-8 Emulator");

  // Chip8 Audio device
  InitAudioDevice();
  Sound beep = LoadSound(argv[2]);

  SetTargetFPS(60);

  // Timing (in Hz)
  const int instructionsPerSecond = 500;
  int delay = 1000000 / instructionsPerSecond;

  while (!WindowShouldClose()) {
    // Update
    if (IsKeyPressed(KEY_P)) {
      PlaySound(beep);
    }

    for (int i = 0; i < delay; i++) {
      Emulate(chip8);
    }

    // Draw
    BeginDrawing();

    for (int x = 0; x < 64; x++) {
      for (int y = 0; y < 32; y++) {
        if (chip8->display[y * 64 + x] != 0) {
          DrawRectangle(x * scale, y * scale, scale, scale, WHITE);
        }
      }
    }

    EndDrawing();
  }

  UnloadSound(beep);

  CloseAudioDevice();

  CloseWindow();
  return 0;
}
