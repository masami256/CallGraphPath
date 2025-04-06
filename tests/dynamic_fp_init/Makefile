CC=clang

all:
	$(CC) -c dynamic_fp_init.c -O0 -emit-llvm -S -g
	llvm-as dynamic_fp_init.ll -o dynamic_fp_init.bc

clean:
	rm -f dynamic_fp_init.ll dynamic_fp_init.bc