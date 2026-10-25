; task.asm - Context switch for BenmoshOS
; Called as: switch_context(task_t* current, task_t* next)
; Arguments on stack: [esp+4] = current PCB ptr, [esp+8] = next PCB ptr


[bits 32]
section .text
global switch_context

; C Signature: void switch_context(uint32_t* current_esp_ptr, uint32_t new_esp, uint32_t new_cr3);
;
; Stack layout on entry:
;   [esp + 0]  = Return EIP
;   [esp + 4]  = current_esp_ptr  (pointer to uint32_t)
;   [esp + 8]  = new_esp          (uint32_t)
;   [esp + 12] = new_cr3          (uint32_t)
switch_context:
	; On entry stack: [ESP] = current_esp_ptr, [ESP+4] = new_esp

	; Push all registers onto the stack to save the current context
	pushfd	; save EFLAGS
	pushad	; save EAX, EBX, ECX, EDX, ESP, EBP, ESI, EDI

	; Save the current ESP directly into the pointer provided by C
	mov eax, [esp + 40]		; get current_esp_ptr - 32 (pushad) + 4 (pushfd) + 4 (return address) = 40
	mov [eax], esp			; save CPU ESP into current_esp_ptr

	mov edx, [esp + 44]     ; edx = new_esp
	mov eax, [esp + 48]     ; eax = new_cr3

	; check if we needto flish the TLB by comparing the current CR3 with the new CR3
	mov eax, cr3			; Read current CR3 into eax
	cmp eax, ecx			; Compare current CR3 with new_cr3
	je .skip_cr3_flush

	mov cr3, ecx	; Load the new CR3 to switch page directories

.skip_cr3_flush:
	; Load the new_esp
	mov esp, edx		; move to the new_esp

	; Pop all registers from the stack to restore the next context
	popad	; restore EAX, EBX, ECX, EDX, ESP, EBP, ESI, EDI
	popfd	; restore EFLAGS

	; Return to the next task's execution
	ret