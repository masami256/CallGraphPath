#include <stdio.h>
#include <stdlib.h>

__attribute__((noinline)) static void myfoo(void) {
    printf("foo\n");
}

__attribute__((noinline)) static void mybar(void) {
    printf("bar\n");
}

__attribute__((noinline)) static void mybaz(void) {
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
    .foo = myfoo,
    .bar = mybar,
};

static void (*sfp)(void) = mybaz;

__attribute__((noinline))  static void test_func(struct inode *inode) {
    printf("test_func\n");
    inode->i_op->foo(); // call myfoo
    inode->i_op->bar(); // call mybar
}

int main(int argc, char **argv) {
    void (*fp)(void) = mybar;

    struct inode i = {
        .i_op = &iops,
    };

    test_func(&i);
    sfp(); // call mybaz()
    fp(); // call mybar()
    return 0;
}