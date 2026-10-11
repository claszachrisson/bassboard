#include "lvgl.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <assert.h>

// Keep the LVGL object/style/text heap out of the RAM needed by audio and DMA.
// The LCD draw/conversion buffers are allocated separately in internal RAM.
void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void* memory, size_t bytes)
{
    (void) memory; (void) bytes; return NULL;
}
void lv_mem_remove_pool(lv_mem_pool_t pool) { (void) pool; }
void* lv_malloc_core(size_t bytes)
{
    return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
void* lv_realloc_core(void* memory, size_t bytes)
{
    return heap_caps_realloc(memory, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
void lv_free_core(void* memory) { heap_caps_free(memory); }
void lv_mem_monitor_core(lv_mem_monitor_t* monitor)
{
    memset(monitor, 0, sizeof(*monitor));
    monitor->total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    monitor->free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    monitor->free_biggest_size = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
}
lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }

void bassboard_ui_memory_init(void)
{
    assert(heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0);
}
