; task.asm - Context switch for BenmoshOS
; Called as: switch_context(task_t* current, task_t* next)
; Arguments on stack: [esp+4] = current PCB ptr, [esp+8] = next PCB ptr

[bits 32]
global switch_context

switch_context:
	; On entry stack: [ESP] = return address, [ESP+4] = current PCB ptr, [ESP+8] = next PCB ptr

	; Push all registers onto the stack to save the current context
	pushfd	; save EFLAGS
	pushad	; save EAX, EBX, ECX, EDX, ESP, EBP, ESI, EDI

	; Save the current stack pointer (ESP) into the current PCB
	;	current->esp = ESP
	mov eax, [esp + 40]	; 32 (pushad) + 4 (pushfd) + 4 (return address) = 40
	mov [eax + 4], esp		; task.esp is at offset 4

	; Load the next stack pointer (ESP) from the next PCB
	mov eax, [esp + 44]	; 32 (pushad) + 4 (pushfd) + 4 (return address) + 4 (current PCB ptr) = 44
	mov esp, [eax + 4]		; task.esp is at offset 4

	; Pop all registers from the stack to restore the next context
	popad	; restore EAX, EBX, ECX, EDX, ESP, EBP, ESI, EDI
	popfd	; restore EFLAGS

	; Return to the next task's execution
	ret