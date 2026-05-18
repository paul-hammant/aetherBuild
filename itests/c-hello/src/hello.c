#include <stdio.h>

int greet_count(void);

int main(void) {
    printf("hello from aeb lib/c (%d sources linked)\n", greet_count());
    return 0;
}
