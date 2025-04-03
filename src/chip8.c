#include "chip8.h"
#include <fcntl.h>
#include <raylib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// buffer size for read call
#define BUFSIZE 4096

const uint8_t FONTS[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

// Create a Chip8 CPU structure.
// Caller is responsible for freeing.
Chip8 *CreateChip8() {
  Chip8 *cpu = (Chip8 *)malloc(sizeof(Chip8));
  if (cpu == NULL) {
    fprintf(stderr, "malloc failed in CreateChip8");
    return NULL;
  }
  memset(cpu->memory, 0, sizeof(cpu->memory));
  // The original CHIP-8 interpreter occupied the first 512 bytes of memory,
  // so most programs started at location 512 (0x200).
  cpu->pc = 0x200;
  cpu->indexReg = 0x0;
  memset(cpu->reg, 0, sizeof(cpu->reg));
  memset(cpu->display, 0, sizeof(cpu->display));
  memset(cpu->stack, 0, sizeof(cpu->stack));
  cpu->sp = 0x0;
  cpu->delayTimer = 0x0;
  cpu->soundTimer = 0x0;
  memset(cpu->keypad, 0, sizeof(cpu->keypad));

  for (int i = 0; i < 80; i++) {
    cpu->memory[i + 0x0] = FONTS[i];
    // printf("Font data: %x\n", cpu->memory[i + 0x0]);
  }
  cpu->playAudio = false;
  return cpu;
}

// Free a Chip8 CPU structure
void FreeChip8(Chip8 *chip8) {
  if (chip8 == NULL) {
    fprintf(stderr, "failed to free chip8");
    return;
  }
  free(chip8);
}

// Load a Rom into a previously-allocated Chip8 structure
bool LoadRom(Chip8 *cpu, const char *path) {
  int fd, tempFd;
  ssize_t bytesRead;
  char buf[BUFSIZE];
  // step 1: load the file
  // a) Identify the file with its fd returned by open()
  // b) For now, read the entire fd and just output the stream to stdio
  if ((fd = open(path, O_RDWR)) < 0) {
    fprintf(stderr, "open failed in LoadRom");
    close(fd);
    return false;
  }

  // create temp file for writing
  if ((tempFd = creat("temp_ch8_copy.bin", 0644)) < 0) {
    fprintf(stderr, "creat failed in LoadRom");
    return false;
  }

  // read bytesRead from fd into buf
  while ((bytesRead = read(fd, buf, BUFSIZE)) > 0) {
    if (write(tempFd, buf, bytesRead) != bytesRead) {
      fprintf(stderr, "write failed in LoadRom");
      close(fd);
      close(tempFd);
      return false;
    }
    unsigned int memOffset = 0x200;

    if (memOffset + bytesRead > 4096) {
      fprintf(stderr, "rom larger than memory");
      close(fd);
      close(tempFd);
      return false;
    }

    // copy bytesRead bytes from the buffer into memory (starting at 0x200)
    memcpy(&cpu->memory[memOffset], buf, bytesRead);
    printf("%x\n", cpu->memory[memOffset]);
    memOffset += bytesRead;
  }

  if (bytesRead < 0) {
    fprintf(stderr, "read failed in LoadRom");
    close(fd);
    close(tempFd);
    return false;
  }

  close(fd);
  close(tempFd);

  return true;
}

// Perform the Fetch-Decode-Execute loop with a Chip8 CPU.
void Emulate(Chip8 *cpu) {
  // Fetch -> Instruction is 2 bytes
  // Read two successive bytes into memory, and then combine them into a 16-bit
  // instruction. Then, increment PC by 2, and be ready to fetch the next
  // opcode.
  // Note that the opcodes are big-endian.
  //
  // Decode -> Decode the instruction based on the first hex value.
  // This'll be a large switch statement of CHIP-8 instructions.
  // Every instruction will have a "first nibble" to say *what* kind of
  // instruction it is, and the rest will have different meanings.
  //
  //  X: The second nibble. Used to look up one of the 16 registers (VX) from V0
  //  through VF.
  //  Y: The third nibble. Also used to look up one of the 16
  //  registers (VY) from V0 through VF.
  //  N: The fourth nibble. A 4-bit number.
  //  NN: The second byte (third and fourth nibbles). An 8-bit immediate number.
  //  NNN: The second, third and fourth nibbles. A 12-bit immediate memory
  //  address.
  //
  // Execute -> Execute the instruction inside the decode stage.
  // Need to read *two* bytes in (the PC).
  // Ex: 0xA2F3 -> memory[pc] = 0xA2; memory[pc + 1] = 0xF3.
  // Grab A2, (the first part), and left shift to place in upper bytes.
  // Then, combine the two.
  uint16_t opcode;
  if (cpu->pc < sizeof(cpu->memory)) {
    // NOTE: Fetch
    opcode = cpu->memory[cpu->pc] << 8 | cpu->memory[cpu->pc + 1];
    cpu->pc += 2;

    uint8_t firstNibble = OPCODE_FIRST(opcode);
    switch (firstNibble) {
    case 0x0: {
      uint16_t last12Bits = opcode & 0x0FFF;
      switch (last12Bits) {
      case 0x00E0:
        printf("CLEAR BACKGROUND\n");
        ClearBackground(RAYWHITE);
        break;
      case 0x00EE:
        printf("RETURN SUBROUNTINE\n");
        // pop last address from stack (decr sp)
        // set pc to sp
        break;
      default:
        // fprintf(stderr, "unknown opcode at %04x at 0x0000", opcode);
        printf("ignore\n");
        break;
      }
      break;
    }
    case 0x1: {
      cpu->pc = OPCODE_NNN(opcode);
      break;
    }
    case 0x2: {
      break;
    }
    case 0x3: {
      break;
    }
    case 0x4: {
      break;
    }
    case 0x5: {
      break;
    }
    case 0x6: {
      printf("SET VX TO NN\n");
      cpu->reg[OPCODE_X(opcode)] = OPCODE_NN(opcode);
      break;
    }
    case 0x7: {
      printf("ADD NN TO VX\n");
      cpu->reg[OPCODE_X(opcode)] += OPCODE_NN(opcode);
      break;
    }
    case 0x8: {
      break;
    }
    case 0x9: {
      break;
    }
    case 0xA: {
      printf("MEM SET I\n");
      cpu->indexReg = OPCODE_NNN(opcode);
      break;
    }
    case 0xB: {
      break;
    }
    case 0xC: {
      break;
    }
    case 0xD: {
      printf("DISPLAY\n");
      // Draw an N pixel tall sprite from mem location I, at horizontal
      // coordinate X in VX and Y coordinate in VY.
      // All pixels that are "on" will *flip* the pixels that it is drawn to.
      // I.e., from l->r; msb -> lsb
      // If *any* pixels on screen were turned off, VF reg will be set to 1.
      // Otherwise, 0.
      uint16_t spriteX = cpu->reg[OPCODE_X(opcode)];
      uint16_t spriteY = cpu->reg[OPCODE_Y(opcode)];
      uint16_t spriteHeight = OPCODE_N(opcode);
      printf("spriteX, spriteY, height: %d %d %d\n", spriteX, spriteY,
             spriteHeight);

      cpu->reg[0xF] = 0;

      // Grab N'th byte of sprite data up to N.
      for (uint16_t row = 0; row < spriteHeight; row++) {
        // Draw a sprite from mem location I
        uint16_t sprite = cpu->memory[cpu->indexReg + row];
        // Sprite's width is always 8
        for (uint16_t col = 0; col < 8; col++) {
          // Extract pixel by MSB
          uint8_t pixel = (sprite >> (7 - col)) & 1;
          // uint8_t pixel = sprite & (0x80 >> col);

          // "Wrap around" x and y (otherwise we will draw off the screen)
          uint16_t x = spriteX & 63;
          uint16_t y = spriteY & 31;
          uint16_t index = y * 64 + x;

          // Current pixel is on AND pixel at (x, y) is on, turn the pixel
          // off.

          /*
          if (pixel && cpu->display[y * 64 + x]) {
            cpu->reg[0xF] = 1;
          } else if (pixel) {
            cpu->display[y * 64 + x] ^= 1;
          }
            */
          if (pixel) {
            if (cpu->display[index]) {
              cpu->reg[0xF] = 1;
            }
            cpu->display[index] ^= 1;
          }
          break;
        }
      case 0xE: {
        break;
      }
      case 0xF: {
        break;
      }
      }
    }
    }
  }

  if (cpu->soundTimer > 0) {
    if (cpu->soundTimer) {
      cpu->playAudio = true;
    }
    cpu->soundTimer--;
  }
}
