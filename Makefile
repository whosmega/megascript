all: main.o chunk.o memory.o debug.o value.o vm.o compiler.o
	gcc main.o chunk.o memory.o debug.o value.o vm.o compiler.o -lm -o out


main.o chunk.o memory.o debug.o value.o vm.o compiler.o: main.c chunk.c memory.c debug.c value.c vm.c compiler.c
	gcc -c main.c chunk.c memory.c debug.c value.c vm.c compiler.c

clean:
	rm main.o
	rm chunk.o
	rm memory.o
	rm debug.o
	rm value.o
	rm vm.o
	rm compiler.o
	rm out
	
