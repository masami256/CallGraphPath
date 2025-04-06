CC=clang

all:
	$(CC) -c static_fp_init.c -O0 -emit-llvm -S -g
	llvm-as static_fp_init.ll -o static_fp_init.bc

clean:
	rm -f static_fp_init.ll static_fp_init.bc