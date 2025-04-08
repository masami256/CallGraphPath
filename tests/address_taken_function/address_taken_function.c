#include <stdio.h>

__attribute__((noinline)) static void foo(int n) {
    printf("foo: %d\n", n);
}

__attribute__((noinline)) static void bar(int n, void (*fp)(int)) {
    n += 1;
    fp(n);
}
int main(void) {
    bar(10, &foo);
    return 0;
}