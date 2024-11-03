SRCS=main.c
BIN=main
CFLAGS=-Wall -lm -O3
SDL2FLAGS=`sdl2-config --cflags --libs`

all:
	gcc -o $(BIN) $(CFLAGS) $(SRCS) $(SDL2FLAGS)

run: all
	./$(BIN)

clean:
	rm -f $(BIN)

.PHONY: all clean run
