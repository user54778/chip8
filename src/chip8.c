#include "chip8.h"
#include <fcntl.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
    // Read two successive bytes into memory, and then combine them into a
    // 16-bit instruction. Then, increment PC by 2, and be ready to fetch the
    // next opcode.
    opcode = cpu->memory[cpu->pc] << 8 | cpu->memory[cpu->pc + 1];
    cpu->pc += 2;

    uint8_t firstNibble = OPCODE_FIRST(opcode);
    switch (firstNibble) {
    case 0x0: {
      // uint16_t last12Bits = opcode & 0x0FFF;
      uint16_t lastEight = OPCODE_NN(opcode);
      switch (lastEight) {
      case 0xE0:
        printf("CLEAR BACKGROUND\n");
        ClearBackground(RAYWHITE);
        break;
      case 0xEE:
        printf("RETURN SUBROUNTINE\n");
        // pop last address from stack (decr sp)
        cpu->sp++;
        // set pc to sp
        cpu->pc = cpu->stack[cpu->sp];
        break;
      default:
        fprintf(stderr, "unknown opcode at %04x at 0x0000\n", opcode);
        return;
      }
      break;
    }
    case 0x1: {
      // Jump to NNN
      cpu->pc = OPCODE_NNN(opcode);
      break;
    }
    case 0x2: {
      // Set pc to top of stack
      cpu->stack[cpu->sp] = cpu->pc;
      // Push to stack
      cpu->sp--;
      // Call subroutine at NNN
      cpu->pc = OPCODE_NNN(opcode);
      break;
    }
    case 0x3: {
      printf("SKIP VX == NN\n");
      if (cpu->reg[OPCODE_X(opcode)] == OPCODE_NN(opcode)) {
        cpu->pc += 2;
      }
      break;
    }
    case 0x4: {
      printf("SKIP VX != NN\n");
      if (cpu->reg[OPCODE_X(opcode)] != OPCODE_NN(opcode)) {
        cpu->pc += 2;
      }
      break;
    }
    case 0x5: {
      printf("SKIP VX == VY\n");
      if (cpu->reg[OPCODE_X(opcode)] == cpu->reg[OPCODE_Y(opcode)]) {
        cpu->pc += 2;
      }
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
      uint8_t lastFour = OPCODE_N(opcode);
      switch (lastFour) {
      case 0x0:
        // Set VX to VY
        printf("8XY0\n");
        cpu->reg[OPCODE_X(opcode)] = cpu->reg[OPCODE_Y(opcode)];
        break;
      case 0x1:
        // Set VX |= VY
        printf("8XY1\n");
        cpu->reg[OPCODE_X(opcode)] |= cpu->reg[OPCODE_Y(opcode)];
        break;
      case 0x2:
        // &=
        printf("8XY2\n");
        cpu->reg[OPCODE_X(opcode)] &= cpu->reg[OPCODE_Y(opcode)];
        break;
      case 0x3:
        // ^=
        printf("8XY3\n");
        cpu->reg[OPCODE_X(opcode)] ^= cpu->reg[OPCODE_Y(opcode)];
        break;
      case 0x4:
        printf("8XY4\n");
        // VF is set to 1 when there's an overflow, and to 0 when there is not.
        uint8_t vxA, vyA;
        vxA = cpu->reg[OPCODE_X(opcode)];
        vyA = cpu->reg[OPCODE_Y(opcode)];
        uint8_t flagA = (vyA > (0xFF - vxA)) ? 1 : 0;
        cpu->reg[OPCODE_X(opcode)] += cpu->reg[OPCODE_Y(opcode)];
        cpu->reg[0xF] = flagA;
        // 7 happy, 6 carry, 7 carry, E carry
        break;
      case 0x5:
        // VF is set to 0 when there's an underflow, and 1 when there is not.
        // VF needs to be set based on the operations result, but also (the
        // original) value needs to be used as an operand if VF is used.
        printf("8XY5\n");
        uint8_t vx, vy;
        vx = cpu->reg[OPCODE_X(opcode)];
        vy = cpu->reg[OPCODE_Y(opcode)];

        uint8_t flag = (vx >= vy) ? 1 : 0;
        cpu->reg[OPCODE_X(opcode)] -= cpu->reg[OPCODE_Y(opcode)];
        cpu->reg[0xF] = flag;
        break;
      case 0x6:
        // VX >> 1
        // Then stores the least significant bit of VX prior to the shift into
        // VF
        // INSTRUCTION GROUP: 8XY6
        // Shift VY one bit right, load VX with result, load VF with bit 0
        printf("8XY6\n");
        flag = cpu->reg[OPCODE_X(opcode)] & 0x1;
        cpu->reg[OPCODE_Y(opcode)] >>= 1;
        cpu->reg[OPCODE_X(opcode)] = cpu->reg[OPCODE_Y(opcode)];
        cpu->reg[0xF] = flag;
        break;
      case 0x7:
        // VX = VY - VX
        printf("8XY7\n");
        uint8_t vx7, vy7;
        vx7 = cpu->reg[OPCODE_X(opcode)];
        vy7 = cpu->reg[OPCODE_Y(opcode)];
        flag = (vy7 >= vx7) ? 1 : 0;

        cpu->reg[OPCODE_X(opcode)] =
            cpu->reg[OPCODE_Y(opcode)] - cpu->reg[OPCODE_X(opcode)];
        cpu->reg[0xF] = flag;
        break;
      case 0xE:
        // VX << 1
        printf("8XYE\n");
        flag = (cpu->reg[OPCODE_X(opcode)] & 0x80) >> 7;
        cpu->reg[OPCODE_X(opcode)] <<= 1;
        cpu->reg[0xF] = flag;
        break;
      }
      break;
    }
    case 0x9: {
      printf("SKIP VX != VY\n");
      if (cpu->reg[OPCODE_X(opcode)] != cpu->reg[OPCODE_Y(opcode)]) {
        cpu->pc += 2;
      }
      break;
    }
    case 0xA: {
      printf("MEM SET I\n");
      cpu->indexReg = OPCODE_NNN(opcode);
      break;
    }
    case 0xB: {
      printf("JUMP NNN + V0\n");
      cpu->pc = OPCODE_NNN(opcode) + cpu->reg[0x0];
      break;
    }
    case 0xC: {
      printf("SET VX RAND & NN");
      int r = rand() % 256;
      cpu->reg[OPCODE_X(opcode)] = r & OPCODE_NN(opcode);
      break;
    }
    case 0xD:
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
        printf("Sprite %d\n", sprite);
        // Sprite's width is always 8
        for (uint16_t col = 0; col < 8; col++) {
          // Extract pixel by MSB
          uint8_t pixel = (sprite >> (7 - col)) & 1;
          // printf("pixel: %08b\n", pixel);
          //  uint8_t pixel = sprite & (0x80 >> col);

          // "Wrap around" x and y (otherwise we will draw off the screen)
          uint16_t x = spriteX & 63;
          uint16_t y = spriteY & 31;
          // This was the bug. We want the wrap around values PLUS the actual
          // row and col values (offset by 64 to index)
          uint16_t index = (((y + row) * 64) + x + col);
          // printf("Index %d\n", index);

          // Current pixel is on AND pixel at (x, y) is on, turn the pixel
          // off.
          if (!(pixel ^ cpu->display[index])) {
            cpu->reg[0xF] = 1;
          } else if (pixel) {
            cpu->display[index] ^= 1;
          }
        }
      }
      break;
    case 0xE: {
      uint8_t lowerEight = OPCODE_NN(opcode);
      switch (lowerEight) {
      case 0x9E:
        printf("KEYOP 9E\n");
        if (cpu->keypad[cpu->reg[OPCODE_X(opcode)]] != 0) {
          cpu->pc += 2;
        }
        break;
      case 0xA1:
        printf("KEYOP A1\n");
        if (cpu->keypad[cpu->reg[OPCODE_X(opcode)]] == 0) {
          cpu->pc += 2;
        }
        break;
      }
      break;
    }
    case 0xF: {
      uint16_t lastEight = OPCODE_NN(opcode);
      switch (lastEight) {
      case 0x07:
        printf("SET TIMER\n");
        cpu->reg[OPCODE_X(opcode)] = cpu->delayTimer;
        break;
      case 0x0A:
        printf("KEY\n");
        updateKeyPad(cpu);

        bool isPressed = false;
        while (!isPressed) {
          for (uint8_t i = 0; i < 16; i++) {
            if (cpu->keypad[i]) {
              cpu->reg[OPCODE_X(opcode)] = i;
              break;
            }
          }
          break;
        }
        break;
      case 0x15:
        printf("SET TIMER VX\n");
        cpu->delayTimer = cpu->reg[OPCODE_X(opcode)];
        break;
      case 0x18:
        printf("SET SOUND VX\n");
        cpu->soundTimer = cpu->reg[OPCODE_X(opcode)];
        break;
      case 0x1E:
        printf("ADD VX I\n");
        cpu->indexReg += cpu->reg[OPCODE_X(opcode)];
        break;
      case 0x29:
        printf("SET I SPRITE ADDR\n");
        break;
      case 0x33:
        // Store binary-coded decimal representation of VX, with
        // the hundreds digit at I, tens at I + 1, and ones at I + 2.
        printf("STORE BIN-CODE DECIMAL OF VX\n");
        // TODO: speed up
        cpu->memory[cpu->indexReg] = cpu->reg[OPCODE_X(opcode)] / 100;
        cpu->memory[cpu->indexReg + 1] = (cpu->reg[OPCODE_X(opcode)] / 10) % 10;
        cpu->memory[cpu->indexReg + 2] = cpu->reg[OPCODE_X(opcode)] % 10;
        break;
      case 0x55:
        printf("DUMP V0 TO VX\n");
        // Store from reg0 to regX, starting at indexReg.
        for (uint16_t i = 0; i <= OPCODE_X(opcode); i++) {
          cpu->memory[cpu->indexReg + i] = cpu->reg[i];
        }
        break;
      case 0x65:
        printf("LOAD V0 TO VX\n");
        for (uint16_t i = 0; i <= OPCODE_X(opcode); i++) {
          cpu->reg[i] = cpu->memory[cpu->indexReg + i];
        }
        break;
      }
      break;
    }
    }
  }

  if (cpu->soundTimer > 0) {
    printf("sound timer: %d\n", cpu->soundTimer);
    if (cpu->soundTimer) {
      cpu->playAudio = true;
    }
    cpu->soundTimer--;
  }
}

void updateKeyPad(Chip8 *cpu) {
  // Chip8 keypad
  // 1 2 3 C
  // 4 5 6 D
  // 7 8 9 E
  // A 0 B F
  //
  // 1 2 3 4
  // Q W E R
  // A S D F
  // Z X C V
  cpu->keypad[0x1] = IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1);
  cpu->keypad[0x2] = IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2);
  cpu->keypad[0x3] = IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3);
  cpu->keypad[0xC] = IsKeyPressed(KEY_FOUR) || IsKeyPressed(KEY_KP_4);

  cpu->keypad[0x4] = IsKeyPressed(KEY_Q);
  cpu->keypad[0x5] = IsKeyPressed(KEY_W);
  cpu->keypad[0x6] = IsKeyPressed(KEY_E);
  cpu->keypad[0xD] = IsKeyPressed(KEY_R);

  cpu->keypad[0x7] = IsKeyPressed(KEY_A);
  cpu->keypad[0x8] = IsKeyPressed(KEY_S);
  cpu->keypad[0x9] = IsKeyPressed(KEY_D);
  cpu->keypad[0xE] = IsKeyPressed(KEY_F);

  cpu->keypad[0xA] = IsKeyPressed(KEY_Z);
  cpu->keypad[0x0] = IsKeyPressed(KEY_X);
  cpu->keypad[0xB] = IsKeyPressed(KEY_C);
  cpu->keypad[0xF] = IsKeyPressed(KEY_V);
}
