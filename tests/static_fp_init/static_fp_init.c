#include <stdio.h>
#include <stdlib.h>

__attribute__((noinline)) static void foo(void) {
    printf("foo\n");
}

__attribute__((noinline)) static void bar(void) {
    printf("bar\n");
}

__attribute__((noinline)) static void baz(void) {
    printf("baz\n");
}

struct inode_operations {
    void (*foo)(void);
    void (*bar)(void);
};

struct inode {
    struct inode_operations *i_op;
};

static struct inode_operations iops = {
    .foo = foo,
    .bar = bar,
};

static void (*sfp)(void) = baz;

__attribute__((noinline))  static void test_func(struct inode *inode) {
    printf("test_func\n");
    inode->i_op->foo();
    inode->i_op->bar();
}

int main(int argc, char **argv) {
    void (*fp)(void) = bar;

    struct inode i = {
        .i_op = &iops,
    };

    test_func(&i);
    sfp();
    fp();
    return 0;
}