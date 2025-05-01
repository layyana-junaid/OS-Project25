CC=gcc
CFLAGS=`pkg-config --cflags gtk+-3.0` -Wall -Wextra -g -pthread
LDFLAGS=`pkg-config --libs gtk+-3.0` -lcurl -pthread

SRC=main.c downloader.c ui.c
OBJ=$(SRC:.c=.o)
TARGET=downloader_gui

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET) 