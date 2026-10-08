#include "task.h"

#include "../../memory/pmm/pmm.h"
#include "../../memory/vmm/vmm.h"
#include "../../memory/heap/heap.h"

static task_t task_table[MAX_TASKS];
static uint32_t task_count	 = 0;
static uint32_t current_task = 0;
static uint32_t next_pid	 = 1;

void task_init(void) {
	/*
	 * Task 0 is the currently-running kernel - it need a PCB too.
	 * Because the task is already running and having a stack we dont need to set it.
	 * switch_context will fill the rest needed in the stack 
	 */
	task_table[0].pid = next_pid++;
	task_table[0].state = TASK_RUNNING;
	task_table[0].stack_phys = 0;	// kernel original stack 
	task_table[0].stack_top = 0;	// not tracked
	task_count = 1;
}

task_t* task_create(void (*entry_point)(void)) {

	// check if there is place for more task
	if (task_count >= MAX_TASKS) {
		return NULL;
	}

	// get a pointer to the next available struct in the array
	task_t* new_task = &task_table[task_count];

	// alocate aligned 4KB stack for the task
	uint32_t* stack = (uint32_t*)kmalloc_aligned(STACK_SIZE, 16);
	if (stack == NULL) {
		return NULL;
	}

	// put the stack at the end of the stack space (stack grows down)
	uint32_t* sp = (uint32_t*)((uint32_t)stack + STACK_SIZE);

	// put return EIP at the start of the stack
	*(--sp) = (int32_t)entry_point;

	// then the eflags
	*(--sp) = 0x202;
	
	// set the 8 general purpose registers
    *(--sp) = 0; // EAX
    *(--sp) = 0; // ECX
    *(--sp) = 0; // EDX
    *(--sp) = 0; // EBX
    *(--sp) = 0; // ESP (Ignored by popad, but takes up 4 bytes of space)
    *(--sp) = 0; // EBP
    *(--sp) = 0; // ESI
    *(--sp) = 0; // EDI

	// save the new stack into the PCB
	new_task->esp = (uint32_t)sp;

	// set the metadata
	task_table[task_count].stack_phys = (uint32_t)stack;
	task_table[task_count].stack_top = (uint32_t)sp;
	task_table[task_count].pid = new_task->pid;
	task_table[task_count].state = new_task->state;

	task_count++;

	return new_task;
}

// Find the next READY task in the circular array
static void schedule() {
	uint32_t original_task = current_task;

	if (task_table[current_task].state == TASK_RUNNING) {
		task_table[current_task].state = TASK_READY;
	}

	do {
		current_task = (current_task + 1) % task_count;

		if (task_table[current_task].state == TASK_READY) {
			break;
		}
	} while (current_task != original_task);

	task_table[current_task].state = TASK_READY;
}

void task_yield() {
	// Disable interrupts so PIT timer cant fire while swapping
	__asm__ volatile("cli");

	uint32_t old_task = current_task;

	// will find next task to run and set the current_task to that new task
	schedule();

	if (current_task != old_task) {
		// do context switch to the new task
		switch_context(&task_table[old_task].esp, &task_table[current_task].esp);
	}

	// abable interrupts
	__asm__ volatile("sti");
}

task_t* task_current() {

	return &task_table[current_task];
}
