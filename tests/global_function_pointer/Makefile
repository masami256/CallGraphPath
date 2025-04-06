CC=clang

all:
	$(CC) -c global_function_pointer.c -O0 -emit-llvm -S -g
	llvm-as global_function_pointer.ll -o global_function_pointer.bc

clean:
	rm -f global_function_pointer.ll global_function_pointer.bc