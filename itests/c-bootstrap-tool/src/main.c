/* The main program — consumes both headers produced by the gen
   codegen node. Their directory is on the include path because the
   .build.ae has a build.dep edge to gen/.build.ae and lib/c adds the
   dependency's published c_header_dirs as -I flags. */
#include <stdio.h>
#include "gen_values.h"
#include "gen_names.h"

int main(void) {
    printf("aeb bootstrap-tool: magic=0x%X version=%d answer=%d\n",
           GEN_MAGIC, GEN_VERSION, GEN_ANSWER);
    for (int i = 0; i < GEN_NAMES_COUNT; i++) {
        printf("  name[%d] = %s\n", i, GEN_NAMES[i]);
    }
    return GEN_ANSWER == 42 ? 0 : 1;
}
