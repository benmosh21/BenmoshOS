#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "../../memory/pmm/pmm.h"
#include "../../memory/vmm/vmm.h"
#include "../../memory/heap/heap.h"

#define STACK_SIZE 4096
#define MAX_TASKS 16	// For now 16 is good enough

extern void switch_context(uint32_t* current_esp_ptr, uint32_t new_esp_ptr);


typedef enum {
	TASK_READY,
	TASK_RUNNING,
	TASK_BLOCKED,
	TASK_DEAD
} task_state_t;

typedef struct {
	// CPU state
	uint32_t eip;
	uint32_t esp;
	uint32_t eax, ebx, ecx, edx;
	uint32_t elfags;
	uint32_t cs, ds, ss;

	// Kernel metadata
	uint32_t pid;
	task_state_t state;

	// Stack memory
	uint32_t stack_phys;
	uint32_t stack_top;
} task_t;

void 	task_init(void);
task_t* task_create(void (*entry_point)(void));
void 	task_yield(void);
task_t* task_current(void);

#endif // TASK_H