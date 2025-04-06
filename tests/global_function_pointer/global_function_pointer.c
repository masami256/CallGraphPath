#include <stdio.h>

static void foo(void) {
    printf("foo\n");
}

static  void (*fp)(void) = foo;

int main(void) {
    fp();
    return 0;
}