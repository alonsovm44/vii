#include <stdio.h>
#include <stdlib.h>

int* allocates() {
    int* p = malloc(sizeof(int)); // heap allocation
    *p = 42;                      // assign value
    return p; 
}

int main() {
    int stack_var = 10;           // stack allocation
    int* heap_var = allocates();  // heap allocation

    printf("Stack variable address: %p\n", (void*)&stack_var);
    printf("Heap variable address:  %p\n", (void*)heap_var);

    printf("Stack value: %d\n", stack_var);
    printf("Heap value:  %d\n", *heap_var);

    free(heap_var); // ✅ prevent memory leak

    return 0;
}