#include <stdio.h>

__attribute__((noinline)) static void foo(void) {
    printf("foo\n");
}

__attribute__((noinline)) static void bar(void) {
    printf("bar\n");
}

int main(int argc, char **argv) {
    void (*fp)(void) = NULL;
    
    if (argc == 1)
        fp = foo;
    else
        fp = bar;
        
    fp();
    return 0;
}