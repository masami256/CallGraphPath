#include <stdio.h>

static void foo(void) {
    printf("foo\n");
}

int main(void) {
    void (*fp)(void) = foo;

    fp();
    return 0;
}