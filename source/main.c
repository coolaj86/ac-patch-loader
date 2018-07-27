#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "aclib.h"

extern void* nesinfo_data_start;
extern void* my_malloc_current[2];
extern void* my_zelda_malloc_align(unsigned long size, int align);
extern void my_zelda_free(void *ptr);
extern void* game_move_first(void *current_graph_process_data);

void load_patches(void *patch_data) {
    // flag bits reserved for future use
    uint16_t global_flags = *(uint16_t*) patch_data;
    patch_data += sizeof(global_flags);

    OSReport("global flags: 0x%x",  global_flags);

    // check if enable JUTConsole flag is set, and enable it if so
    if (global_flags & 1) {
        OSReport("enabling JUTConsole without zurumode");
        *((uint32_t *)&game_move_first + 6) = 0x60000000; // nop the branch if equal
        ICInvalidateRange(game_move_first, 0x20);
        JUTReportConsole("JUT Console is now enabled!");
    }

    // get number of patches to load
    uint16_t num_patches = *(uint16_t*) patch_data;
    patch_data += sizeof(num_patches);

    OSReport("loading %u patches", num_patches);

    for (uint16_t i = 0; i < num_patches; i++) {
        uint32_t target_addr = *(uint32_t*) patch_data;
        patch_data += sizeof(target_addr);

        uint32_t patch_size = *(uint32_t*) patch_data;
        patch_data += sizeof(patch_size);

        uint32_t flags = *(uint32_t*) patch_data;
        patch_data += sizeof(flags);

        void *content = patch_data;
        patch_data += patch_size;

        OSReport("patch #%u: 0x%x bytes to %p", i, patch_size, target_addr);

        if (target_addr < 0x80000000 || target_addr + patch_size >= 0x81800000) {
            OSReport("invalid target %p", target_addr);
            continue;
        }

        memcpy((void*) target_addr, content, patch_size);

        // Invalidate both the Instruction & Data caches, just to be safe
        ICInvalidateRange((void*) target_addr, patch_size);
        DCInvalidateRange((void*) target_addr, patch_size);

        if (flags & 1) {
            OSReport("jump flag set; jumping to %p", target_addr);
            void (*jump_to)() = (void*) target_addr;
            jump_to();
        }
    }
}

/*
 * Hooks call to my_zelda_free
 */
void __entry(void *ptr) {
    OSReport("NES TAG LOADER CALLED");

    if (nesinfo_data_start) {
        OSReport("loading from nesinfo_data_start @ %p", nesinfo_data_start);
        load_patches(nesinfo_data_start);
    } else {
        OSReport("ERROR: nesinfo_data_start is NULL");
    }

    OSReport("NES TAG LOADER EXITING");

    // Restore the my_malloc pointer
    my_malloc_current[1] = &my_zelda_free;

    // Do the original function call
    my_zelda_free(ptr);
}
