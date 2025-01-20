# Compiler and flags
CC = gcc
CFLAGS = -Wall -O2
LDFLAGS =

# Source files
SRC = i2c-hori-controller.c
OBJ = $(SRC:.c=.o)

# Output binary
TARGET = i2c-hori-controller

# Rules
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)