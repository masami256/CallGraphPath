CC=clang

all:
	$(CC) -c address_taken_function.c -O0 -emit-llvm -S -g
	llvm-as address_taken_function.ll -o address_taken_function.bc

clean:
	rm -f address_taken_function.ll address_taken_function.bc