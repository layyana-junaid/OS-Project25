# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g $(shell pkg-config --cflags gtk+-3.0) -Iheaders
LDFLAGS = $(shell pkg-config --libs gtk+-3.0) -lpthread -lcurl

# Source files
SRC = ui/ui.c \
      src/downloader.c \
      src/network.c \
      src/segment.c \
      src/writer.c \
      src/monitor.c

OBJ = $(SRC:.c=.o)

# Executable name
TARGET = bin/gui_downloader

# Default target
all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p bin
	$(CC) -o $@ $^ $(LDFLAGS)

# Pattern rule for compiling .c files to .o files
%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -rf $(OBJ) $(TARGET) obj

.PHONY: all clean
