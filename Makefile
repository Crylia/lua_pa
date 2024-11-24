# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC -I/usr/include/pulseaudio -I/usr/include/lua5.3
LDFLAGS = -lpulse -llua5.3 -lm

# Project files and directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# List of source files
SRCS = $(wildcard $(SRC_DIR)/*.c)

# Generate object files from source files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Shared library output name
TARGET = $(BIN_DIR)/lua_pa.so

# Default rule: build the shared library
all: $(TARGET)

# Rule to compile .c files into .o object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to link the object files into the shared library
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJS) -shared -o $(TARGET) $(LDFLAGS)

# Clean up build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Install the shared library
install: $(TARGET)
	cp $(TARGET) /usr/local/lib/

.PHONY: all clean install
