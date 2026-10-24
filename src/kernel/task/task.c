#include "task.h"
#include "../../memory/pmm/pmm.h"
#include "../../memory/vmm/vmm.h"
#include "../../memory/heap/heap.h"
#include "../tss/tss.h"
#include "../../cpu/ports/ports.h"

static LIST_HEAD(ready_list);
LIST_HEAD(all_tasks_list);
static LIST_HEAD(dead_list);

static task_t* 	current_task_ptr = NULL;
static task_t 	kernel_task;// The kernel task PCB
static uint32_t task_count	 = 0;
static uint32_t next_pid	 = 1;

static void task_exit_trampoline(void (*entry_point)(void)) {
    /* Enable interrupts now that we are executing the task context */
    __asm__ volatile("sti");
    
    entry_point();
    
    task_exit();
}

void task_init(void) {
	/*
	 * Task 0 is the currently-running kernel - it need a PCB too.
	 * Because the task is already running and having a stack we dont need to set it.
	 * switch_context will fill the rest needed in the stack 
	 */

	current_task_ptr = &kernel_task;

	kernel_task.pid 		= next_pid++;
	kernel_task.state 		= TASK_RUNNING;
	kernel_task.stack_base 	= 0;	// kernel original stack
	kernel_task.stack_top  	= 0x90000;
	kernel_task.esp 		= 0; // will be filled by switch_context
	kernel_task.ticks 		= 0;
	
	list_init(&kernel_task.list);
	list_init(&kernel_task.all_list);

	list_add_tail(&kernel_task.all_list, &all_tasks_list);

	current_task_ptr = &kernel_task;
	task_count = 1;
	
}

task_t* task_create(void (*entry_point)(void)) {

	uint32_t flags = irq_save();

	// get a pointer to the next available struct in the array
	task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
	if (new_task == NULL) {
		irq_restore(flags);
		return NULL;
	}

	// alocate aligned 4KB stack for the task
	uint32_t* stack = (uint32_t*)kmalloc_aligned(STACK_SIZE, 16);
	if (stack == NULL) {
		kfree(new_task);
		irq_restore(flags);
		return NULL;
	}

	// put the stack at the end of the stack space (stack grows down)
	uint32_t* sp = (uint32_t*)((uint32_t)stack + STACK_SIZE);

	// put return EIP at the start of the stack
	*(--sp) = (uint32_t)entry_point;
	*(--sp) = 0; // dummy return address for trampoline
	*(--sp) = (uint32_t)task_exit_trampoline;

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

	list_init(&new_task->list);
	list_init(&new_task->all_list);

	list_add_tail(&new_task->list, &ready_list);
	list_add_tail(&new_task->all_list, &all_tasks_list);

	task_count++;

	irq_restore(flags);
	return new_task;
}


// free dead_tasks memories
static void reap_dead_tasks() {

	struct list_head* pos, *n;

	list_for_each_safe(pos, n, &dead_list) {
		task_t* task = list_entry(pos, task_t, list);

		// Remove the task from the dead list and all_tasks_list
		list_del(&task->list);
		list_del(&task->all_list);

		// Free the stack memory if it was allocated
		if (task->stack_base != 0) {
			kfree_aligned((void*)task->stack_base);
		}
		kfree(task);
		task_count--;
	}

}


// Find the next READY task in the ready_list do context switch to it
void task_yield() {
	// Save the current CPU state and switch to the next READY task
	uint32_t flags = irq_save();

	reap_dead_tasks(); // Clean up any dead tasks after switching

	if (list_empty(&ready_list)) {
		irq_restore(flags);
		return; // No other tasks to switch to
	}

	task_t* old_task = current_task_ptr;

	if (old_task->state == TASK_RUNNING) {
		old_task->state = TASK_READY;
		list_add_tail(&old_task->list, &ready_list);
	}

	task_t* next_task = list_first_entry(&ready_list, task_t, list);
	list_del(&next_task->list);

	next_task->state = TASK_RUNNING;
	next_task->ticks++;
	current_task_ptr = next_task;

	// Update hardware TSS for ring 3 re-entry
	g_tss.esp0 = next_task->stack_top;

	switch_context(&old_task->esp, &next_task->esp);

	// reenable interrupts after context switch is done
	irq_restore(flags);
}

void task_exit() {
	irq_save();

	if (current_task_ptr == &kernel_task) {
		for (;;) {
			__asm__ volatile("hlt");
		}
	}

	current_task_ptr->state = TASK_DEAD;
	list_add_tail(&current_task_ptr->list, &dead_list);

	task_yield();
	
	// should never reach heare
	for (;;) {
        __asm__ volatile("hlt");
    }
}

// kill task, return 0 if success, else return -1
int task_kill(uint32_t pid) {
	uint32_t flags = irq_save();

	if (pid <= 1) {
		irq_restore(flags);
		return -1; // Prevent killing the kernel task
	}

	struct list_head* pos;
	list_for_each(pos, &all_tasks_list) {
		task_t* task = list_entry(pos, task_t, all_list);
		if (task->pid == pid && task->state != TASK_DEAD) {
			if (task == current_task_ptr) {
				task_exit(); // If it's the current task, exit it
			} else {
				list_del(&task->list); // Remove from ready list if it's there
				task->state = TASK_DEAD;
				list_add_tail(&task->list, &dead_list);
			}
			irq_restore(flags);
			return 0;
		}
	}

	irq_restore(flags);
	return -1; // Task with the given PID not found
}

task_t* task_current() {
	return current_task_ptr;
}

uint32_t task_get_count() {
    return task_count;
}
