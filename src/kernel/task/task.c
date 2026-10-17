#include "task.h"
#include "../../memory/pmm/pmm.h"
#include "../../memory/vmm/vmm.h"
#include "../../memory/heap/heap.h"
#include "../tss/tss.h"

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
	task_table[0].stack_base = 0;	// kernel original stack 
	task_table[0].stack_top = 0x90000;
	task_table[0].esp = 0;
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

	*(--sp) = (uint32_t)task_exit;
	// put return EIP at the start of the stack
	*(--sp) = (uint32_t)entry_point;

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
	new_task->stack_base = (uint32_t)stack;
	new_task->stack_top  = (uint32_t)stack + STACK_SIZE;
	new_task->pid 		 = next_pid++;
	new_task->state 	 = TASK_READY;
	new_task->ticks 	 = 0;

	task_count++;

	return new_task;
}


// free dead_tasks memories
static void reap_dead_tasks() {
	for (uint32_t i = 0; i < task_count; i++) {
		if (task_table[i].state == TASK_DEAD && i != current_task) {
			if (task_table[i].stack_base != 0) {
				if ((void*)task_table[i].stack_base != NULL) {
					kfree_aligned((void*)task_table[i].stack_base);
				}
				task_table[i].stack_base = 0;
			}

			for (uint32_t j = i; j < task_count -1; j++) {
				task_table[j] = task_table[j+1];
			}
			task_count--;

			if (current_task > i) {
				current_task--;
			}
			i--;
		}
	}
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

	task_table[current_task].state = TASK_RUNNING;
	task_table[current_task].ticks++;
}

void task_yield() {
	// Disable interrupts so PIT timer cant fire while swapping
	__asm__ volatile("cli");

	uint32_t old_task = current_task;

	// will find next task to run and set the current_task to that new task
	schedule();

	if (current_task != old_task) {
		// Update TSS esp0 for the new task
		g_tss.esp0 = task_table[current_task].stack_top;

		// do context switch to the new task
		switch_context(&task_table[old_task].esp, &task_table[current_task].esp);
	}

	// after each switch checking for dead_tasks
	reap_dead_tasks();

	// enable interrupts
	__asm__ volatile("sti");
}

void task_exit() {
	__asm__ volatile("cli");

	if (current_task == 0) {
		for (;;) {
			__asm__ volatile("hlt");
		}
	}

	task_table[current_task].state = TASK_DEAD;

	task_yield();
	
	// should never reach heare
	for (;;) {
        __asm__ volatile("hlt");
    }
}

void task_kill(uint32_t pid) {
	if (pid <= 1) return; // dont remove the kernel

	for (int i = 0 ; i < task_count; i++) {
		if (task_table[i].pid == pid && task_table[i].state != TASK_DEAD) {
			task_table[i].state = TASK_DEAD;
			if (i == current_task) {
				task_yield();
			}
			return;
		}
	}
    
}

task_t* task_current() {
	return &task_table[current_task];
}

task_t *task_get(uint32_t index) {
	if (index >= task_count) return NULL;
    return &task_table[index];
}

uint32_t task_get_count() {
    return task_count;
}
