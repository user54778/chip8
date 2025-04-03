#include "chip8.h"
#include "raylib.h"
#include <stdlib.h>
#include <unistd.h>

#define SCALE 10

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

  // Chip8 window
  InitWindow(screenWidth * SCALE, screenHeight * SCALE, "CHIP-8 Emulator");

  // Chip8 Audio device
  InitAudioDevice();
  Sound beep = LoadSound(argv[2]);

  SetTargetFPS(60);

  // Timing (in Hz)
  const int instructionsPerSecond = 500;
  int delay = 1000000 / instructionsPerSecond;

  while (!WindowShouldClose()) {
    // Update
    Emulate(chip8);

    if (chip8->playAudio) {
      PlaySound(beep);
      chip8->playAudio = false;
    }

    // Draw
    BeginDrawing();

    ClearBackground(BLACK);

    DrawScreen(chip8);

    EndDrawing();

    usleep(delay);
  }

  UnloadSound(beep);

  CloseAudioDevice();

  CloseWindow();
  return 0;
}

void DrawScreen(const Chip8 *chip8) {
  for (uint16_t row = 0; row < 32; row++) {
    for (uint16_t col = 0; col < 64; col++) {
      // Each row has 64 pixels
      // To find the index in our 1D flattened 2D array, we should
      // multiply row * width, and then add the column offset to get to the
      // pixel.
      if (chip8->display[row * 64 + col]) {
        DrawRectangle(col * SCALE, row * SCALE, SCALE, SCALE, WHITE);
      }
    }
  }
}
