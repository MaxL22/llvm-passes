#include <stdio.h>
#include <stdint.h>

void __log_indir(const char* src_info, uint32_t src_line, uintptr_t dst_addr) {
    FILE *f = fopen("indir_log.txt", "a");
    if (f) {
        fprintf(f, "Source: %s:%u -> Dest: 0x%lx\n", src_info, src_line, (unsigned long)dst_addr);
        fclose(f);
    }
}
