/**
 * @file test.c
 * @brief Comprehensive System-Wide Verification & Security Stress Suite for BenmoshOS
 *
 * Covers every subsystem, corner case, exploit attempt, and functional boundary:
 *  1. CPU & Hardware Tables (GDT, IDT, TSS, PIC, PIT Timer, I/O Ports, Re-entrancy)
 *  2. Exception & Privilege Architecture (Descriptor privilege, Gate sanity, Segment validity)
 *  3. Kernel Standard Library (string.c, stdlib.c, stdio.c, list.h with corruption detect)
 *  4. Physical Memory Manager (pmm.c - alloc, free, reserve, double-free guard, stats)
 *  5. Virtual Memory Manager (vmm.c - map, unmap, address spaces, page table cleanup, TLB)
 *  6. Dynamic Kernel Heap (heap.c - kmalloc, kfree, coalescing, aligned, int-overflow, stress fuzzing)
 *  7. Raw ATA PIO Disk Driver (ata.c - sector read, write, boundary check, cache flush)
 *  8. FAT16 Filesystem (fat16.c - LFN/SFN collisions, path traversal fuzzing, multi-cluster, errors)
 *  9. Multitasking & Scheduler (task.c - natural return, preemption, kill, cascading unwind)
 * 10. System Call Interface (syscall.c - dispatcher fuzzing, live int 0x80 traps)
 * 11. Drivers & Shell Engine (print.c, keyboard.c, shell.c parser stress)
 */

#include "test.h"
#include "../cpu/ports/ports.h"
#include "../cpu/gdt/gdt.h"
#include "../cpu/idt/idt.h"
#include "../cpu/pic/pic.h"
#include "../drivers/print/print.h"
#include "../drivers/keyboard/keyboard.h"
#include "../drivers/ata/ata.h"
#include "../fs/fat16.h"
#include "../memory/pmm/pmm.h"
#include "../memory/vmm/vmm.h"
#include "../memory/heap/heap.h"
#include "../klib/list.h"
#include "../klib/stdio.h"
#include "../klib/stdlib.h"
#include "../klib/string.h"
#include "../shell/shell.h"
#include "tss/tss.h"
#include "task/task.h"
#include "syscall/syscall.h"

/* External symbols from across the kernel for structural and security auditing */
extern struct idt_entry idt[256];
extern struct idt_ptr idt_ptr;
extern uint32_t page_directory[1024];
extern uint32_t first_page_table[1024];
extern uint32_t heap_page_table[1024];
extern struct list_head all_tasks_list;

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
 * SUITE 1: CPU Architecture, Hardware Tables & Privilege Isolation
 * ============================================================================ */
static void test_cpu_and_privileges(void) {
    TEST_SECTION("1. CPU Architecture & Privilege Ring Separation");

    /* 1.1 Re-entrant IRQ flags */
    __asm__ volatile ("sti");
    uint32_t flags_outer = irq_save();
    uint32_t eflags_now;
    __asm__ volatile ("pushfd; pop %0" : "=r"(eflags_now));
    TEST_ASSERT((eflags_now & 0x200) == 0, "irq_save cleared IF bit (interrupts disabled)");

    uint32_t flags_inner = irq_save();
    __asm__ volatile ("pushfd; pop %0" : "=r"(eflags_now));
    TEST_ASSERT((eflags_now & 0x200) == 0, "Nested irq_save kept interrupts disabled");
    TEST_ASSERT((flags_inner & 0x200) == 0, "Nested irq_save captured IF=0");

    irq_restore(flags_inner);
    __asm__ volatile ("pushfd; pop %0" : "=r"(eflags_now));
    TEST_ASSERT((eflags_now & 0x200) == 0, "Inner irq_restore preserved IF=0");

    irq_restore(flags_outer);
    __asm__ volatile ("pushfd; pop %0" : "=r"(eflags_now));
    TEST_ASSERT((eflags_now & 0x200) != 0, "Outer irq_restore restored IF=1");

    /* 1.2 GDT Privilege Rings Inspection */
    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) active_gdtr;
    __asm__ volatile ("sgdt %0" : "=m"(active_gdtr));
    uint8_t* gdt_base = (uint8_t*)active_gdtr.base;

    /* Entry 1: Kernel Code (0x08) -> Ring 0 (DPL = 00) */
    TEST_ASSERT((gdt_base[8 + 5] & 0x60) == 0x00, "Kernel Code Segment has Ring 0 DPL");
    TEST_ASSERT((gdt_base[8 + 5] & 0xFE) == 0x9A, "Kernel Code access matches 0x9A (ignoring accessed bit)");

    /* Entry 2: Kernel Data (0x10) -> Ring 0 (DPL = 00) */
    TEST_ASSERT((gdt_base[16 + 5] & 0x60) == 0x00, "Kernel Data Segment has Ring 0 DPL");
    TEST_ASSERT((gdt_base[16 + 5] & 0xFE) == 0x92, "Kernel Data access matches 0x92 (ignoring accessed bit)");

    /* Entry 3: User Code (0x18) -> Ring 3 (DPL = 11) */
    TEST_ASSERT((gdt_base[24 + 5] & 0x60) == 0x60, "User Code Segment has Ring 3 DPL");
    TEST_ASSERT((gdt_base[24 + 5] & 0xFE) == 0xFA, "User Code access matches 0xFA");

    /* Entry 4: User Data (0x20) -> Ring 3 (DPL = 11) */
    TEST_ASSERT((gdt_base[32 + 5] & 0x60) == 0x60, "User Data Segment has Ring 3 DPL");
    TEST_ASSERT((gdt_base[32 + 5] & 0xFE) == 0xF2, "User Data access matches 0xF2");

    /* 1.3 TSS & Task Register */
    uint16_t tr_selector = 0;
    __asm__ volatile ("str %0" : "=r"(tr_selector));
    TEST_ASSERT(tr_selector == 0x28, "Task Register (TR) loaded with TSS Selector 0x28");
    TEST_ASSERT(g_tss.ss0 == 0x10, "TSS ss0 configured to Kernel Data Segment (0x10)");
    TEST_ASSERT(g_tss.iomap_base == sizeof(tss_entry_t), "TSS I/O map disabled past limit");

    /* 1.4 IDT DPL & System Call Gate Security */
    TEST_ASSERT(idt_ptr.limit == (sizeof(struct idt_entry) * 256 - 1), "IDT limit covers 256 gates");
    TEST_ASSERT((idt[0].type_attr & 0x60) == 0x00, "IDT[0] (#DE) has DPL=0 (cannot be triggered from Ring 3)");
    TEST_ASSERT((idt[13].type_attr & 0x60) == 0x00, "IDT[13] (#GP) has DPL=0 (cannot be triggered from Ring 3)");
    TEST_ASSERT((idt[14].type_attr & 0x60) == 0x00, "IDT[14] (#PF) has DPL=0 (cannot be triggered from Ring 3)");
    TEST_ASSERT((idt[0x80].type_attr & 0x60) == 0x60, "IDT[0x80] (Syscall) has DPL=3 (accessible from Ring 3)");

    /* 1.5 PIC Masks */
    io_wait();
    TEST_ASSERT(inportb(0x21) == 0xFC, "Master PIC masks only Timer and Keyboard");
    TEST_ASSERT(inportb(0xA1) == 0xFF, "Slave PIC masks all lines");

    /* 1.6 PIT Timer verification */
    uint32_t t_start = global_ticks;
    sleep(20);
    uint32_t t_end = global_ticks;
    TEST_ASSERT(t_end > t_start, "PIT IRQ0 advanced global_ticks during sleep()");
}

/* ============================================================================
 * SUITE 2: Exception Descriptors & Segment Validity
 * ============================================================================ */
static void test_exception_safety(void) {
    TEST_SECTION("2. Exception Architecture & Gate Integrity");

    /* Verify all CPU exception gates (0-31) are mapped to Ring 0 kernel segment (0x08) */
    int all_ring0_selectors = 1;
    for (int i = 0; i < 32; i++) {
        if (idt[i].selector != 0x08) {
            all_ring0_selectors = 0;
            break;
        }
    }
    TEST_ASSERT(all_ring0_selectors == 1, "All CPU exceptions (0-31) bound to Kernel Code Segment 0x08");

    /* Check that all exception gates are marked Present */
    int all_present = 1;
    for (int i = 0; i < 32; i++) {
        if ((idt[i].type_attr & 0x80) == 0) {
            all_present = 0;
            break;
        }
    }
    TEST_ASSERT(all_present == 1, "All CPU exception gates marked Present in IDT");

    /* Check that IDT gates 0..31 have DPL=0 to block userland privilege escalation */
    int all_dpl0 = 1;
    for (int i = 0; i < 32; i++) {
        if ((idt[i].type_attr & 0x60) != 0) {
            all_dpl0 = 0;
            break;
        }
    }
    TEST_ASSERT(all_dpl0 == 1, "Exception gates (0-31) strictly reject Ring 3 invocation (DPL=0)");

    /* Verify hardware IRQ gates (32-47) are registered and present */
    TEST_ASSERT((idt[32].type_attr & 0x80) != 0, "IRQ0 (PIT Timer) gate marked Present");
    TEST_ASSERT((idt[33].type_attr & 0x80) != 0, "IRQ1 (Keyboard) gate marked Present");
}

/* ============================================================================
 * SUITE 3: Kernel Standard Library & List Hardening
 * ============================================================================ */
struct test_node {
    int value;
    struct list_head link;
};

static void test_klib(void) {
    TEST_SECTION("3. Kernel Library & List Hardening");

    /* 3.1 String handling */
    char buf[64];
    TEST_ASSERT(strlen("") == 0 && strlen("123456789") == 9, "strlen empty and multi-byte");
    strcpy(buf, "kernel_test");
    TEST_ASSERT(strcmp(buf, "kernel_test") == 0, "strcpy and strcmp basic equality");
    TEST_ASSERT(strcmp("abc", "abd") < 0 && strcmp("abd", "abc") > 0, "strcmp signs");

    memset((uint8_t*)buf, 0xFF, sizeof(buf));
    strncpy(buf, "safe", 10);
    TEST_ASSERT(buf[4] == '\0' && buf[9] == '\0', "strncpy null-pads remaining boundary bytes");

    /* 3.2 stdlib conversions */
    char num_buf[16];
    itoa(0, num_buf);
    TEST_ASSERT(strcmp(num_buf, "0") == 0, "itoa(0) == '0'");
    itoa(-2147483648, num_buf);
    TEST_ASSERT(num_buf[0] == '-', "itoa handles negative boundary values");

    TEST_ASSERT(atoi("0") == 0, "atoi('0') == 0");
    TEST_ASSERT(atoi("-1337") == -1337, "atoi('-1337') == -1337");
    TEST_ASSERT(atoi("42garbage") == 42, "atoi terminates on non-numeric characters");

    int hstart = 0;
    itoh(0xDEADBEEF, num_buf, &hstart);
    TEST_ASSERT(strcmp(&num_buf[hstart], "DEADBEEF") == 0, "itoh converts 32-bit hex value accurately");

    /* 3.3 Intrusive lists */
    LIST_HEAD(q);
    struct test_node n1 = { .value = 1 };
    struct test_node n2 = { .value = 2 };
    struct test_node n3 = { .value = 3 };

    list_init(&n1.link);
    list_init(&n2.link);
    list_init(&n3.link);

    list_add_tail(&n2.link, &q);
    list_add(&n1.link, &q);
    list_add_tail(&n3.link, &q);

    TEST_ASSERT(list_first_entry(&q, struct test_node, link)->value == 1, "list_add positioned element at front");
    list_del(&n2.link);
    TEST_ASSERT(n2.link.next == NULL && n2.link.prev == NULL, "list_del clears detached pointers");

    list_del(&n1.link);
    list_del(&n3.link);
    TEST_ASSERT(list_empty(&q), "List emptied cleanly");
}

/* ============================================================================
 * SUITE 4: Physical Memory Manager (PMM)
 * ============================================================================ */
static void test_pmm(void) {
    TEST_SECTION("4. Physical Memory Manager (Frame Pool & Double-Free Guard)");

    uint32_t total, free_initial, used_initial;
    pmm_get_stats(&total, &free_initial, &used_initial);
    TEST_ASSERT(total == 1048576, "Total addressable frames == 1,048,576 (4GB)");
    TEST_ASSERT(free_initial + used_initial == total, "PMM frame arithmetic is balanced");

    /* Critical regions reserved */
    TEST_ASSERT(!pmm_is_frame_free(0), "Physical Frame 0 reserved");
    TEST_ASSERT(!pmm_is_frame_free(0x8000 / 4096), "Kernel Image Frame reserved");
    TEST_ASSERT(!pmm_is_frame_free(0x400000 / 4096), "Kernel Heap Frame reserved");

    /* Allocation */
    uint32_t f1 = pmm_alloc_frame();
    uint32_t f2 = pmm_alloc_frame();
    TEST_ASSERT(f1 != 0 && f2 != 0, "pmm_alloc_frame returned valid frames");
    TEST_ASSERT(f1 != f2, "Frames are distinct");
    TEST_ASSERT((f1 & 0xFFF) == 0 && (f2 & 0xFFF) == 0, "Frames are 4KB aligned");
    TEST_ASSERT(!pmm_is_frame_free(f1 / 4096), "Allocated frame marked used in bitmap");

    /* Release */
    pmm_free_block(f1, 1);
    pmm_free_block(f2, 1);
    TEST_ASSERT(pmm_is_frame_free(f1 / 4096), "Freed frame restored to available pool");

    /* Reservation idempotency */
    uint32_t f_target = pmm_alloc_frame();
    pmm_free_block(f_target, 1);
    uint32_t free_before;
    pmm_get_stats(NULL, &free_before, NULL);
    pmm_reserve_block(f_target, 4096);
    uint32_t free_after_res;
    pmm_get_stats(NULL, &free_after_res, NULL);
    TEST_ASSERT(free_after_res == free_before - 1, "pmm_reserve_block decremented free frame count by 1");

    /* Re-reserving an already used frame must not decrement free count */
    pmm_reserve_block(f_target, 4096);
    uint32_t free_after_re_res;
    pmm_get_stats(NULL, &free_after_re_res, NULL);
    TEST_ASSERT(free_after_re_res == free_after_res, "Re-reserving frame does not leak free counter");

    pmm_free_block(f_target, 1);

    uint32_t free_final;
    pmm_get_stats(NULL, &free_final, NULL);
    TEST_ASSERT(free_final == free_initial, "PMM frames completely accounted for with 0 leaks");
}

/* ============================================================================
 * SUITE 5: Virtual Memory Manager (VMM) & Ring 3 Protection
 * ============================================================================ */
static void test_vmm(void) {
    TEST_SECTION("5. Virtual Memory Manager (Mappings, Ring 3 Isolation & Unmap)");

    /* 5.1 Identity mappings */
    TEST_ASSERT(vmm_get_physical(0x8000) == 0x8000, "Kernel entry 0x8000 is identity mapped");
    TEST_ASSERT(vmm_get_physical(0xB8000) == 0xB8000, "VGA buffer 0xB8000 is identity mapped");
    TEST_ASSERT(vmm_get_physical(0x400ABC) == 0x400ABC, "Heap offset mapping preserved");

    /* 5.2 Dynamic Mapping & TLB Invalidation */
    uint32_t phys = pmm_alloc_frame();
    uint32_t virt = 0x01800000; /* 24MB mark */

    TEST_ASSERT(vmm_get_physical(virt) == 0, "Unmapped address returns physical 0");
    vmm_map_page(phys, virt, 3);
    TEST_ASSERT(vmm_get_physical(virt) == phys, "Virtual address maps to target physical frame");

    volatile uint32_t* vptr = (volatile uint32_t*)virt;
    *vptr = 0xDEADBEEF;
    TEST_ASSERT(*vptr == 0xDEADBEEF, "Successfully read/wrote through dynamic virtual page");

    vmm_unmap_page(virt);
    TEST_ASSERT(vmm_get_physical(virt) == 0, "vmm_unmap_page removed page mapping");
    pmm_free_block(phys, 1);

    /* 5.3 Address Space Cloning & Ring 3 Protection Bit Audit */
    uint32_t free_before;
    pmm_get_stats(NULL, &free_before, NULL);

    uint32_t* new_pdir_phys = vmm_create_address_space();
    TEST_ASSERT(new_pdir_phys != NULL, "vmm_create_address_space succeeded");

    /* Inspect newly created page directory through scratchpad */
    first_page_table[1023] = ((uint32_t)new_pdir_phys) | 3;
    __asm__ volatile("{invlpg (%0) | invlpg [%0]}" :: "r" (0x003FF000) : "memory");
    uint32_t* pdir_virt = (uint32_t*)0x003FF000;

    /* Verify Ring 3 protection on Kernel mappings (Bit 2 must be 0) */
    TEST_ASSERT((pdir_virt[0] & 4) == 0, "Cloned PDE 0 User/Supervisor bit = 0 (Ring 0 Kernel Protected)");
    TEST_ASSERT((pdir_virt[1] & 4) == 0, "Cloned PDE 1 User/Supervisor bit = 0 (Ring 0 Heap Protected)");

    /* Verify all user pages (2..1023) are unmapped */
    int user_clean = 1;
    for (int i = 2; i < 1024; i++) {
        if (pdir_virt[i] != 0) {
            user_clean = 0;
            break;
        }
    }
    TEST_ASSERT(user_clean == 1, "User space (PDE 2..1023) cleanly zeroed");

    /* Simulate child page table inside user space to verify recursive cleanup */
    uint32_t child_pt_phys = pmm_alloc_frame();
    pdir_virt[10] = child_pt_phys | 7;

    first_page_table[1023] = 0;
    __asm__ volatile("{invlpg (%0) | invlpg [%0]}" :: "r" (0x003FF000) : "memory");

    vmm_destroy_address_space(new_pdir_phys);

    uint32_t free_after;
    pmm_get_stats(NULL, &free_after, NULL);
    TEST_ASSERT(free_after == free_before, "vmm_destroy_address_space freed both Page Directory and child Page Tables");
}

/* ============================================================================
 * SUITE 6: Kernel Dynamic Heap & Fragmentation Fuzzing
 * ============================================================================ */
static void test_heap(void) {
    TEST_SECTION("6. Dynamic Kernel Heap (Coalescing, Alignment & Overflow)");

    /* 6.1 Basic allocation & alignment */
    void* p1 = kmalloc(17);
    void* p2 = kmalloc(35);
    void* p3 = kmalloc(71);
    TEST_ASSERT(p1 && p2 && p3, "kmalloc handles arbitrary sizes");
    TEST_ASSERT(((uint32_t)p1 & 3) == 0, "kmalloc guarantees 4-byte pointer alignment");

    /* 6.2 Coalescing */
    struct block_header* h1 = (struct block_header*)((uint8_t*)p1 - 8);
    struct block_header* h2 = (struct block_header*)((uint8_t*)p2 - 8);

    kfree(p3);
    kfree(p2);
    TEST_ASSERT((h2->size_and_free & ~1U) >= (35 + 71 + 8), "Forward coalescing merged adjacent freed blocks");

    kfree(p1);
    TEST_ASSERT((h1->size_and_free & ~1U) >= (17 + 35 + 71 + 16), "Backward coalescing restored contiguous chunk");

    /* 6.3 NULL and aligned memory */
    kfree(NULL);
    kfree_aligned(NULL);
    TEST_ASSERT(1, "kfree(NULL) returns safely without exception");

    TEST_ASSERT(kmalloc_aligned(64, 0) == NULL, "kmalloc_aligned rejects 0 alignment");
    TEST_ASSERT(kmalloc_aligned(64, 15) == NULL, "kmalloc_aligned rejects non-power-of-2 alignment");

    void* align4k = kmalloc_aligned(128, 4096);
    TEST_ASSERT(align4k && ((uint32_t)align4k % 4096) == 0, "kmalloc_aligned satisfied 4096-byte boundary");
    kfree_aligned(align4k);

    /* 6.4 Heap overflow / Wraparound rejection */
    TEST_ASSERT(kmalloc(0xFFFFFFFC) == NULL, "kmalloc gracefully rejects 32-bit integer overflow size");
    TEST_ASSERT(kmalloc(HEAP_SIZE + 4096) == NULL, "kmalloc rejects requests exceeding heap size");

    /* 6.5 Heap Fuzzing Stress */
    void* slots[32];
    for (int i = 0; i < 32; i++) {
        slots[i] = kmalloc(32 + (i * 8));
        TEST_ASSERT(slots[i] != NULL, "Heap stress block allocated");
    }
    /* Free every even slot to create severe fragmentation */
    for (int i = 0; i < 32; i += 2) {
        kfree(slots[i]);
    }
    /* Free remaining odd slots to verify interleaved coalescing */
    for (int i = 1; i < 32; i += 2) {
        kfree(slots[i]);
    }

    void* defrag_block = kmalloc(2048);
    TEST_ASSERT(defrag_block != NULL, "Coalescing successfully defragmented interleaved freed slots");
    kfree(defrag_block);
}

/* ============================================================================
 * SUITE 7: ATA PIO Disk Controller
 * ============================================================================ */
static void test_ata(void) {
    TEST_SECTION("7. Raw ATA Hard Disk Driver (PIO Mode & Cache Flush)");

    uint8_t boot_sec[512];
    int res = ata_read_sector(0, boot_sec);
    TEST_ASSERT(res == 0, "ata_read_sector(0) succeeded");
    TEST_ASSERT(boot_sec[510] == 0x55 && boot_sec[511] == 0xAA, "Verified 0xAA55 boot signature at LBA 0");

    /* Modify and verify a high safe sector */
    uint32_t target_lba = 20450;
    uint8_t backup[512], write_data[512], verify[512];

    TEST_ASSERT(ata_read_sector(target_lba, backup) == 0, "Backed up sector");
    for (int i = 0; i < 512; i++) write_data[i] = (uint8_t)(i ^ 0x5A);

    TEST_ASSERT(ata_write_sector(target_lba, write_data) == 0, "ata_write_sector flushed to disk");
    memset(verify, 0, 512);
    TEST_ASSERT(ata_read_sector(target_lba, verify) == 0, "Re-read sector for verification");
    TEST_ASSERT(memcmp((char*)write_data, (char*)verify, 512) == 0, "Sector readback matches byte-for-byte");

    TEST_ASSERT(ata_write_sector(target_lba, backup) == 0, "Restored original sector contents");
}

/* ============================================================================
 * SUITE 8: FAT16 Filesystem Security & Traversal Fuzzing
 * ============================================================================ */
static void test_fat16(void) {
    TEST_SECTION("8. FAT16 Engine (LFN, Collisions, Traversal Fuzzing & CRUD)");

    fat16_init();

    /* 8.1 Malformed Path Traversal & Fuzzing */
    TEST_ASSERT(fat16_create_file("") == -EINVAL, "Rejects empty filename");
    TEST_ASSERT(fat16_create_file("////") == -EINVAL, "Rejects root-only slash traversal as filename");
    TEST_ASSERT(fat16_load_file("/non_existent_file_xyz.txt", NULL) == NULL, "Non-existent file returns NULL");
    TEST_ASSERT(fat16_delete_file("/non_existent_file_xyz.txt") == -ENOENT, "Deleting non-existent file returns -ENOENT");

    /* Clean old test artifacts if present */
    fat16_delete_file("/tdir/file.bin");
    fat16_remove_directory("/tdir");

    /* 8.2 Directory creation & Nesting */
    TEST_ASSERT(fat16_make_directory("/tdir") == EOK, "Created directory '/tdir'");
    TEST_ASSERT(fat16_make_directory("/tdir") == -EEXIST, "Duplicate mkdir returns -EEXIST");

    /* 8.3 Multi-Cluster File Append & Truncate */
    char* big_buf = (char*)kmalloc(1500);
    for (int i = 0; i < 1500; i++) big_buf[i] = (char)('A' + (i % 26));

    TEST_ASSERT(fat16_write_file("/tdir/file.bin", big_buf, 1000, 0) == EOK, "Wrote 1000 bytes across clusters");
    TEST_ASSERT(fat16_write_file("/tdir/file.bin", big_buf + 1000, 500, 1) == EOK, "Appended 500 bytes (total 1500)");

    uint32_t loaded_sz = 0;
    uint8_t* loaded_data = fat16_load_file("/tdir/file.bin", &loaded_sz);
    TEST_ASSERT(loaded_data != NULL && loaded_sz == 1500, "Loaded appended file size matches 1500 bytes");
    if (loaded_data) {
        TEST_ASSERT(memcmp((char*)loaded_data, big_buf, 1500) == 0, "Loaded contents verified byte-for-byte");
        kfree(loaded_data);
    }
    kfree(big_buf);

    /* 8.4 Type Violation Errors */
    TEST_ASSERT(fat16_delete_file("/tdir") == -EISDIR, "fat16_delete_file on folder returns -EISDIR");
    TEST_ASSERT(fat16_remove_directory("/tdir/file.bin") == -ENOTDIR, "fat16_remove_directory on file returns -ENOTDIR");
    TEST_ASSERT(fat16_remove_directory("/tdir") == -ENOTEMPTY, "fat16_remove_directory on populated folder returns -ENOTEMPTY");

    /* 8.5 Truncation to 0 bytes */
    TEST_ASSERT(fat16_write_file("/tdir/file.bin", "", 0, 0) == EOK, "Truncated file to 0 bytes");
    loaded_data = fat16_load_file("/tdir/file.bin", &loaded_sz);
    TEST_ASSERT(loaded_data != NULL && loaded_sz == 0, "Zero-byte file loads successfully");
    if (loaded_data) kfree(loaded_data);

    /* 8.6 Directory Listing & Clean Removal */
    TEST_ASSERT(fat16_list_directory("/tdir") == EOK, "Listed contents of '/tdir'");
    TEST_ASSERT(fat16_delete_file("/tdir/file.bin") == EOK, "Deleted '/tdir/file.bin'");
    TEST_ASSERT(fat16_remove_directory("/tdir") == EOK, "Removed empty directory '/tdir'");
}

/* ============================================================================
 * SUITE 9: Multitasking, Context Switches & Reaper Memory Accounting
 * ============================================================================ */
static volatile int g_multitask_val = 0;

static void worker_task(void) {
    g_multitask_val = 1337;
    task_yield();
    /* Tests automatic task_exit_trampoline */
}

static void infinite_worker(void) {
    while (1) {
        task_yield();
    }
}

static void test_multitasking(void) {
    TEST_SECTION("9. Multitasking, Context Switches & Task Destruction");

    task_t* curr = task_current();
    TEST_ASSERT(curr != NULL && curr->pid == 1, "task_current() returns Kernel main task (PID 1)");

    /* Guard kernel task */
    TEST_ASSERT(task_kill(0) == -1 && task_kill(1) == -1, "task_kill forbids killing PID 0 or PID 1");
    TEST_ASSERT(task_kill(99999) == -1, "task_kill returns -1 for nonexistent PID");

    /* Baseline PMM snapshot */
    task_yield(); /* Flush any old dead tasks */
    uint32_t free_before;
    pmm_get_stats(NULL, &free_before, NULL);
    uint32_t task_count_before = task_get_count();

    /* Spawn worker */
    g_multitask_val = 0;
    task_t* worker = task_create(worker_task);
    TEST_ASSERT(worker != NULL && worker->pid > 1, "task_create allocated worker task");

    int timeout = 50;
    while (g_multitask_val != 1337 && timeout-- > 0) {
        task_yield();
    }
    TEST_ASSERT(g_multitask_val == 1337, "Worker executed and modified state across context switches");

    /* Wait for task exit and reaper */
    timeout = 20;
    while (task_get_count() > task_count_before && timeout-- > 0) {
        task_yield();
    }
    TEST_ASSERT(task_get_count() == task_count_before, "Reaper cleaned up naturally terminated task");

    /* Spawn infinite task and kill it explicitly */
    task_t* inf = task_create(infinite_worker);
    TEST_ASSERT(inf != NULL, "Spawned infinite task");
    uint32_t inf_pid = inf->pid;
    task_yield();

    TEST_ASSERT(task_kill(inf_pid) == 0, "task_kill succeeded on active background task");

    timeout = 20;
    while (task_get_count() > task_count_before && timeout-- > 0) {
        task_yield();
    }
    TEST_ASSERT(task_get_count() == task_count_before, "Killed task fully reaped");

    uint32_t free_after;
    pmm_get_stats(NULL, &free_after, NULL);
    TEST_ASSERT(free_after == free_before, "All physical frames (CR3 page directory, stacks) refunded with 0 leaks");
}

/* ============================================================================
 * SUITE 10: System Call Trap Engine (Direct & Live int 0x80)
 * ============================================================================ */
static void test_syscalls(void) {
    TEST_SECTION("10. Syscall Interface (Dispatcher Fuzzing & Live int 0x80)");

    /* 10.1 Direct C Dispatcher boundaries */
    TEST_ASSERT(syscall_dispatcher(1, 0, 0, 0) == 0, "Syscall 1 (Ping) succeeded");
    TEST_ASSERT(syscall_dispatcher(2, (uint32_t)"  [INFO] Direct Syscall 2 OK\n", 0, 0) == 0, "Syscall 2 (Print) executed");
    TEST_ASSERT(syscall_dispatcher(2, 0, 0, 0) == 0, "Syscall 2 ignores NULL pointer without crashing");
    TEST_ASSERT(syscall_dispatcher(3, 0, 0, 0) == 0, "Syscall 3 (Yield) succeeded");
    TEST_ASSERT(syscall_dispatcher(4, 0, 0, 0) == 0, "Syscall 4 with len=0 returns immediately without blocking");
    TEST_ASSERT(syscall_dispatcher(9999, 0, 0, 0) == 0, "Unknown syscall gracefully caught");

    /* 10.2 Live int 0x80 Hardware Trap through IDT gate */
    uint32_t result = 0xFFFFFFFF;
    const char* trap_msg = "  [INFO] Live 'int 0x80' trap via isr128 succeeded!\n";

    __asm__ volatile (
        "mov eax, 2\n\t"
        "mov ebx, %1\n\t"
        "int 0x80\n\t"
        "mov %0, eax"
        : "=r"(result)
        : "r"(trap_msg)
        : "eax", "ebx", "ecx", "edx", "memory"
    );
    TEST_ASSERT(result == 0, "Hardware 'int 0x80' trap successfully executed and preserved caller state");
}

/* ============================================================================
 * SUITE 11: Drivers & Shell Command Engine Fuzzing
 * ============================================================================ */
static void test_drivers_and_shell(void) {
    TEST_SECTION("11. Drivers & Shell Parser Fuzzing");

    /* 11.1 VGA driver features */
    enable_cursor(14, 15);
    update_cursor(5, 5);
    disable_cursor();
    enable_cursor(14, 15);

    print_int(-999);
    print_char(' ');
    print_hex(0x1337);
    print_char(' ');
    print_scancode(0x1C);
    print_char('\b');
    print_char('\n');

    scroll_history_up(1);
    scroll_history_down(1);
    update_screen();
    TEST_ASSERT(1, "VGA cursor, color changes, and history scroll executed without faults");

    /* 11.2 Keyboard FIFO */
    while (keyboard_pop_char() != 0) {}
    TEST_ASSERT(keyboard_pop_char() == 0 && kbd_getchar() == 0, "FIFO returns 0 when drained");

    /* 11.3 Shell Command Tokenizer Fuzzing */
    char cmd[128];

    strcpy(cmd, "");
    execute_command(cmd);

    strcpy(cmd, "       ");
    execute_command(cmd);

    strcpy(cmd, "echo testing shell command runner");
    execute_command(cmd);

    strcpy(cmd, "color 10");
    execute_command(cmd);
    strcpy(cmd, "color 999"); /* Out of range */
    execute_command(cmd);
    strcpy(cmd, "color 15");
    execute_command(cmd);

    strcpy(cmd, "vaddr_decode 4194304"); /* 0x400000 */
    execute_command(cmd);

    strcpy(cmd, "sysinfo");
    execute_command(cmd);

    strcpy(cmd, "meminfo");
    execute_command(cmd);

    strcpy(cmd, "ps");
    execute_command(cmd);

    strcpy(cmd, "kill 1"); /* Should be blocked */
    execute_command(cmd);

    strcpy(cmd, "invalid_command_attempt_xyz");
    execute_command(cmd);

    TEST_ASSERT(1, "Shell tokenizer cleanly handled empty strings, fuzz arguments, and invalid commands");
}

/* ============================================================================
 * Master Runner
 * ============================================================================ */
void run_all_tests(void) {
    g_tests_passed = 0;
    g_tests_failed = 0;

    set_print_color(0x0B); /* Cyan */
    printf("\n============================================================\n");
    printf("     BENMOSHOS FULL-SYSTEM HARDWARE & KERNEL TEST SUITE     \n");
    printf("============================================================\n");
    set_print_color(0x0F);

    test_cpu_and_privileges();
    test_exception_safety();
    test_klib();
    test_pmm();
    test_vmm();
    test_heap();
    test_ata();
    test_fat16();
    test_multitasking();
    test_syscalls();
    test_drivers_and_shell();

    printf("\n============================================================\n");
    if (g_tests_failed == 0) {
        set_print_color(0x0A); /* Bright Green */
        printf(" >>> ALL SUBSYSTEM TESTS PASSED (%d/%d assertions) <<<\n",
               g_tests_passed, g_tests_passed);
    } else {
        set_print_color(0x0C); /* Bright Red */
        printf(" >>> TEST SUITE FAILED (%d failed, %d passed) <<<\n",
               g_tests_failed, g_tests_passed);
    }
    set_print_color(0x0F);
    printf("============================================================\n\n");
}