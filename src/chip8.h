#ifndef CHIP8_H
#define CHIP8_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct chip8 {
  // 4 KB memory buffer
  uint8_t memory[4096];
  // 16-bit program counter to point at current instruction
  uint16_t pc;
  // 16-bit operation code to specify operation being performed
  uint16_t opcode;
  // 16-bit index register that points to locations in memory
  uint16_t indexReg;
  // 16 8-bit general purpose registers
  uint8_t v[16];
  // A 64x32 display buffer
  uint8_t display[64 * 32];
  // A stack for 16-bit addresses
  uint16_t stack[16];
  // A stack pointer for the stack buffer
  uint16_t sp;
  // An 8 bit delay timer decremented at a rate 60 Hz
  uint8_t delayTimer;
  // An 8-bit sound timer akin to the display timer
  uint8_t soundTimer;
  // 16 key hex keypad states
  bool keypad[16];
} Chip8;

extern const uint8_t FONTS[80];

// Create a Chip8 CPU structure
Chip8 *CreateChip8(void);
// Load a Rom into a previously-allocated Chip8 structure
bool LoadRom(Chip8 *, const char *);
// Perform the Fetch-Decode-Execute loop with a Chip8 CPU.
Chip8 *Emulate(Chip8 *);
// Free a Chip8 CPU structure
void FreeChip8(Chip8 *);

#endif
