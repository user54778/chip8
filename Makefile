CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = -lm

# Source files
SRC = main.c chip8.c
OBJ = main.o chip8.o
TARGET = chip8

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

main.o: src/main.c src/chip8.h
	$(CC) -c $(CFLAGS) src/main.c -o main.o

chip8.o: src/chip8.c src/chip8.h
	$(CC) -c $(CFLAGS) src/chip8.c -o chip8.o

clean:
	rm -f $(OBJ) $(TARGET) *.bin

.PHONY: all clean
