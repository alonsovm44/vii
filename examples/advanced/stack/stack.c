#include <stdio.h>

void foo() {
    int x = 10;
    printf("foo: %p\n", (void*)&x);
}

void bar() {
    int y = 20;
    printf("bar: %p\n", (void*)&y);
    foo();
}

int main() {
    int z = 30;
    printf("main: %p\n", (void*)&z);
    bar();
}