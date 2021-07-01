ifeq ($(OS),Windows_NT)     # is Windows_NT on XP, 2000, 7, Vista, 10...
    exe := mega.exe
else
    exe := mega
endif

all:
	gcc -O2 -c src/compiler.c src/table.c src/main.c src/memory.c src/scanner.c src/vm.c src/value.c src/object.c src/chunk.c src/debug.c src/globals.c src/gcollect.c
	gcc -lm -O2 compiler.o main.o memory.o table.o scanner.o vm.o value.o object.o chunk.o debug.o globals.o gcollect.o -o $(exe)


clean:
	rm *.o

clearbin:
	rm $(exe)


