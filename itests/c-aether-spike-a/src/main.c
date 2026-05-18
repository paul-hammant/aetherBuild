#include <stdio.h>

/* Defined in add.ae, surfaced by `aetherc --emit=lib` as a plain C
   function with this exact signature. */
int add_two(int a, int b);

int main(void) {
    int r = add_two(20, 22);
    printf("aeb: C main + Aether add_two(20,22) = %d\n", r);
    return r == 42 ? 0 : 1;
}
