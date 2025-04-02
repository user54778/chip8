#include "chip8.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

#define BUFSIZE 512

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
  cpu->opcode = 0x0;
  cpu->indexReg = 0x0;
  memset(cpu->v, 0, sizeof(cpu->v));
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
      close(tempFd);
      return false;
    }
  }

  if (bytesRead < 0) {
    fprintf(stderr, "read failed in LoadRom");
    close(fd);
    close(tempFd);
    return false;
  }

  close(fd);
  close(tempFd);

  // FIXME: check return value
  system("diff -q '../roms/IBM Logo.ch8' temp_ch8_copy.bin");

  // TODO: load the rom from read in file
  return true;
}

// Perform the Fetch-Decode-Execute loop with a Chip8 CPU.
Chip8 *Emulate(Chip8 *cpu) {
  // TODO: Implement me!
  return cpu;
}
