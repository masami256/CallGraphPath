#include <stdio.h>

static void foo(void) {
    printf("foo\n");
}

__attribute__((noinline)) static void bar(void (*fp)(void)) {
    fp();
}
int main(void) {
    bar(&foo);
    return 0;
}