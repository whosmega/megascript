all:
	gcc -O2 -c src/compiler.c src/main.c src/memory.c src/scanner.c src/vm.c src/value.c src/object.c src/chunk.c src/debug.c
	gcc -lm -O2 compiler.o main.o memory.o scanner.o vm.o value.o object.o chunk.o debug.o -o megascript


clean:
	rm compiler.o 
	rm main.o 
	rm memory.o 
	rm scanner.o 
	rm vm.o 
	rm value.o 
	rm object.o 
	rm chunk.o 
	rm debug.o 

clms:
	rm megascript 


