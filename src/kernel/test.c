#include "test.h"
#include "../cpu/ports/ports.h"
#include "../klib/list.h"
#include "../klib/stdio.h"
#include "../klib/string.h"
#include "../memory/pmm/pmm.h"
#include "../memory/heap/heap.h"
#include "../fs/fat16.h"
#include "task/task.h"
#include "../drivers/print/print.h"

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        g_tests_passed++; \
    } else { \
        g_tests_failed++; \
        set_print_color(0x0C); /* Red */ \
        printf("  [FAIL] Line %d: %s\n", __LINE__, msg); \
        set_print_color(0x0F); \
    } \
} while (0)

#define TEST_SECTION(title) do { \
    set_print_color(0x0E); /* Yellow */ \
    printf("\n=== [TEST SUITE] %s ===\n", title); \
    set_print_color(0x0F); \
} while (0)

/* ============================================================================
 * TEST 1: Re-entrant IRQ Save / Restore
 * ============================================================================ */
static void test_irq_reentrancy(void) {
    TEST_SECTION("1. Re-entrant IRQ State Control");

    __asm__ volatile ("sti");

    uint32_t flags_outer = irq_save();
    uint32_t current_eflags;
    __asm__ volatile ("pushfd; pop %0" : "=r"(current_eflags));
    TEST_ASSERT((current_eflags & 0x200) == 0, "irq_save successfully disabled interrupts");

    uint32_t flags_inner = irq_save();
    __asm__ volatile ("pushfd; pop %0" : "=r"(current_eflags));
    TEST_ASSERT((current_eflags & 0x200) == 0, "Nested irq_save maintained interrupts disabled");
    TEST_ASSERT((flags_inner & 0x200) == 0, "Inner flags captured that interrupts were already disabled");

    irq_restore(flags_inner);
    __asm__ volatile ("pushfd; pop %0" : "=r"(current_eflags));
    TEST_ASSERT((current_eflags & 0x200) == 0, "Restoring inner flags did not prematurely re-enable interrupts");

    irq_restore(flags_outer);
    __asm__ volatile ("pushfd; pop %0" : "=r"(current_eflags));
    TEST_ASSERT((current_eflags & 0x200) != 0, "Restoring outer flags successfully restored interrupt flag");
}

/* ============================================================================
 * TEST 2: Intrusive Doubly Linked Lists & container_of
 * ============================================================================ */
struct dummy_item {
    int id;
    struct list_head list_node;
    int payload;
};

static void test_intrusive_lists(void) {
    TEST_SECTION("2. Intrusive Linked Lists (list_head / container_of)");

    LIST_HEAD(test_queue);
    TEST_ASSERT(list_empty(&test_queue), "List initialized as empty");

    struct dummy_item a = { .id = 1, .payload = 100 };
    struct dummy_item b = { .id = 2, .payload = 200 };
    struct dummy_item c = { .id = 3, .payload = 300 };

    list_init(&a.list_node);
    list_init(&b.list_node);
    list_init(&c.list_node);

    list_add_tail(&a.list_node, &test_queue);
    list_add_tail(&b.list_node, &test_queue);
    list_add_tail(&c.list_node, &test_queue);
    TEST_ASSERT(!list_empty(&test_queue), "List is not empty after appends");

    struct dummy_item* first = list_first_entry(&test_queue, struct dummy_item, list_node);
    TEST_ASSERT(first->id == 1 && first->payload == 100, "list_first_entry returned correct struct via container_of");

    int expected_id = 1;
    struct list_head* pos;
    list_for_each(pos, &test_queue) {
        struct dummy_item* it = list_entry(pos, struct dummy_item, list_node);
        TEST_ASSERT(it->id == expected_id, "Order preserved during FIFO traversal");
        expected_id++;
    }

    list_del(&b.list_node);
    int sum = 0;
    list_for_each(pos, &test_queue) {
        struct dummy_item* it = list_entry(pos, struct dummy_item, list_node);
        sum += it->id;
    }
    TEST_ASSERT(sum == 4, "Middle item unlinked cleanly (sum of 1 and 3 is 4)");

    list_del(&a.list_node);
    list_del(&c.list_node);
    TEST_ASSERT(list_empty(&test_queue), "Queue returned to empty after unlinking all elements");
}

/* ============================================================================
 * TEST 3: Physical Memory Manager (PMM)
 * ============================================================================ */
static void test_pmm(void) {
    TEST_SECTION("3. Physical Memory Manager (PMM Frame Pool)");

    uint32_t total, free_before, used;
    pmm_get_stats(&total, &free_before, &used);
    TEST_ASSERT(free_before > 0, "PMM has free memory frames on boot");

    uint32_t frame1 = pmm_alloc_frame();
    uint32_t frame2 = pmm_alloc_frame();

    TEST_ASSERT(frame1 != 0 && frame2 != 0, "Successfully allocated two physical 4KB frames");
    TEST_ASSERT(frame1 != frame2, "PMM allocated distinct non-overlapping frames");
    TEST_ASSERT((frame1 & 0xFFF) == 0 && (frame2 & 0xFFF) == 0, "Allocated frames are strictly 4096-byte aligned");

    TEST_ASSERT(!pmm_is_frame_free(frame1 / 4096), "Allocated frame is marked used in bitmap");

    pmm_free_block(frame1, 1);
    pmm_free_block(frame2, 1);

    uint32_t free_after;
    pmm_get_stats(&total, &free_after, &used);
    TEST_ASSERT(free_after == free_before, "Free frame count restored after freeing frames");
}

/* ============================================================================
 * TEST 4: Kernel Heap Allocator & Boundary Coalescing
 * ============================================================================ */
static void test_heap(void) {
    TEST_SECTION("4. Dynamic Kernel Heap (kmalloc / kfree / Coalescing)");

    void* p1 = kmalloc(64);
    void* p2 = kmalloc(128);
    void* p3 = kmalloc(256);

    TEST_ASSERT(p1 != NULL && p2 != NULL && p3 != NULL, "Allocated multiple variable-size heap buffers");
    TEST_ASSERT(((uint32_t)p1 % 4) == 0, "Heap pointers are 4-byte aligned");

    memset(p1, 0xAA, 64);
    memset(p2, 0xBB, 128);
    memset(p3, 0xCC, 256);

    uint8_t* b1 = (uint8_t*)p1;
    uint8_t* b2 = (uint8_t*)p2;
    TEST_ASSERT(b1[0] == 0xAA && b1[63] == 0xAA, "Canary intact on buffer 1");
    TEST_ASSERT(b2[0] == 0xBB && b2[127] == 0xBB, "Canary intact on buffer 2");

    void* p_aligned = kmalloc_aligned(1024, 64);
    TEST_ASSERT(p_aligned != NULL, "kmalloc_aligned returned non-null pointer");
    TEST_ASSERT(((uint32_t)p_aligned % 64) == 0, "kmalloc_aligned strictly satisfied 64-byte alignment");
    kfree_aligned(p_aligned);

    kfree(p2);
    kfree(p1);
    kfree(p3);

    void* big_p = kmalloc(64 + 128 + 256);
    TEST_ASSERT(big_p != NULL, "Freed adjacent chunks coalesced successfully to serve large request");
    kfree(big_p);
}

/* ============================================================================
 * TEST 5: FAT16 Multi-Cluster, Path Traversal & Zero-Byte Load
 * ============================================================================ */
static void test_fat16(void) {
    TEST_SECTION("5. FAT16 File System Engine");

    /* 1. Zero-byte file test */
    int res = fat16_create_file("/empty_test.txt");
    TEST_ASSERT(res == EOK || res == -EEXIST, "Created empty test file");

    uint32_t size = 0xFFFFFFFF;
    uint8_t* content = fat16_load_file("/empty_test.txt", &size);
    TEST_ASSERT(content != NULL && size == 0, "Zero-byte file load returned non-null buffer with size 0");
    if (content) kfree(content);

    /* 2. Subdirectory creation & path navigation */
    res = fat16_make_directory("/suite_dir");
    TEST_ASSERT(res == EOK || res == -EEXIST, "Created subdirectory '/suite_dir'");

    /* 3. Multi-cluster file write (spans > 512 bytes) */
    char test_data[1200];
    for (int i = 0; i < 1199; i++) {
        test_data[i] = 'A' + (i % 26);
    }
    test_data[1199] = '\0';

    res = fat16_create_file("/suite_dir/multi.txt");
    TEST_ASSERT(res == EOK || res == -EEXIST, "Created nested file '/suite_dir/multi.txt'");

    res = fat16_write_file("/suite_dir/multi.txt", test_data, 1200, 0);
    TEST_ASSERT(res == EOK, "Wrote 1200 bytes across multiple sectors/clusters");

    /* 4. Verification read */
    uint32_t read_size = 0;
    uint8_t* read_buf = fat16_load_file("/suite_dir/multi.txt", &read_size);
    TEST_ASSERT(read_buf != NULL, "Loaded back nested multi-cluster file");
    TEST_ASSERT(read_size == 1200, "Loaded file size matches exactly 1200 bytes");
    if (read_buf) {
        TEST_ASSERT(memcmp(read_buf, (uint8_t*)test_data, 1200) == 0, "File content verified byte-for-byte");
        kfree(read_buf);
    }

    /* 5. Clean up delete */
    res = fat16_delete_file("/suite_dir/multi.txt");
    TEST_ASSERT(res == EOK, "Deleted nested file successfully");

    res = fat16_remove_directory("/suite_dir");
    TEST_ASSERT(res == EOK, "Removed emptied directory '/suite_dir'");

    res = fat16_delete_file("/empty_test.txt");
    TEST_ASSERT(res == EOK, "Deleted root empty file");
}

/* ============================================================================
 * TEST 6: Intrusive Task Scheduler Queues
 * ============================================================================ */
static volatile int g_task_test_flag = 0;

static void dummy_task_entry(void) {
    g_task_test_flag = 42;
}

static void test_tasking(void) {
    TEST_SECTION("6. Multitasking Runqueue Transitions");

    g_task_test_flag = 0;
    uint32_t count_before = task_get_count();

    task_t* t = task_create(dummy_task_entry);
    TEST_ASSERT(t != NULL, "task_create allocated task via intrusive queue");
    TEST_ASSERT(task_get_count() == count_before + 1, "task_count incremented correctly");

    /* Yield in a bounded loop to let the round-robin scheduler reach dummy_task */
    int timeout = 50;
    while (g_task_test_flag != 42 && timeout-- > 0) {
        task_yield();
    }

    TEST_ASSERT(g_task_test_flag == 42, "Context switched to dummy task and executed its body");

    /* Yield a few times to ensure reap_dead_tasks cleans it up */
    timeout = 10;
    while (task_get_count() > count_before && timeout-- > 0) {
        task_yield();
    }

    TEST_ASSERT(task_get_count() == count_before, "Reaper freed dead task and restored task_count");
}

/* ============================================================================
 * Master Runner
 * ============================================================================ */
void run_all_tests(void) {
    g_tests_passed = 0;
    g_tests_failed = 0;

    test_irq_reentrancy();
    test_intrusive_lists();
    test_pmm();
    test_heap();
    test_fat16();
    test_tasking();

    printf("\n========================================\n");
    if (g_tests_failed == 0) {
        set_print_color(0x0A); /* Bright Green */
        printf(">>> ALL TESTS PASSED (%d/%d assertions) <<<\n", g_tests_passed, g_tests_passed);
    } else {
        set_print_color(0x0C); /* Bright Red */
        printf(">>> TEST SUITE FAILED (%d failures, %d passed) <<<\n", g_tests_failed, g_tests_passed);
    }
    set_print_color(0x0F);
    printf("========================================\n\n");
}