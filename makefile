build:
	gcc -Wall -std=c99 emu8.c -lSDL2 -o emu8 -g 

clean:
	rm emu8