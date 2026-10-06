#include "task.h"


static task_t task_table[MAX_TASKS];
static uint32_t task_count	 = 0;
static uint32_t current_task = 0;
static uint32_t next_pid	 = 1;

void task_init(void) {
	/* Task 0 is the currently-running kernel - it need a PCB too.
	 * Because the task is already running and having a stack we dont need to set it.
	 * switch_context will fill the rest needed in the stack */
	task_table[0].pid = next_pid++;
	task_table[0].state = TASK_RUNNING;
	task_table[0].stack_phys = 0;	// kernel original stack 
	task_table[0].stack_top = 0;	// not tracked
	task_count = 1;
}

task_t* task_create(void, (*entry_point)(void)) {

	return nullptr;
}

void task_yield() {

}

task_t* task_current() {

	return nullptr;
}
