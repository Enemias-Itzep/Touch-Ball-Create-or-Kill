; G8RTOS_SchedulerASM.s
; Holds all ASM functions needed for the scheduler
; Note: If you have an h file, do not have a C file and an S file of the same name

	; Functions Defined
	.def G8RTOS_Start, PendSV_Handler

	; Dependencies
	.ref CurrentlyRunningThread, G8RTOS_Scheduler

	.thumb		; Set to thumb mode
	.align 2	; Align by 2 bytes (thumb mode uses allignment by 2 or 4)
	.text		; Text section

; Need to have the address defined in file 
; (label needs to be close enough to asm code to be reached with PC relative addressing)
RunningPtr: .field CurrentlyRunningThread, 32

; G8RTOS_Start
;	Sets the first thread to be the currently running thread
;	Starts the currently running thread by setting Link Register to tcb's Program Counter
G8RTOS_Start:

	.asmfunc

	;Gets the SP from RunningPtr(**CurrentlyRunningThread)
	ldr r4, RunningPtr
	ldr r5, [r4,#0]
	ldr sp, [r5, #0]

	;Pops registers
	pop {R4-R11}
	pop {R0-R3}
	pop {R12}
	pop {LR}
	pop {LR}
	pop {XPSR}

	;Enable interrupts
	CPSIE I

	;Branches to first thread
	bx LR

	.endasmfunc

; PendSV_Handler
; - Performs a context switch in G8RTOS
; 	- Saves remaining registers into thread stack
;	- Saves current stack pointer to tcb
;	- Calls G8RTOS_Scheduler to get new tcb
;	- Set stack pointer to new stack pointer from new tcb
;	- Pops registers from thread stack
PendSV_Handler:
	
	.asmfunc

	;Disables interrupts
	CPSID I

	;Saves registers
	push {R4-R11}

	;Stores current stack pointer to TCB
	ldr r4, RunningPtr
	ldr r5, [r4,#0]
	str sp, [r5,#0]

	;pushes LR
	push {LR}

	;Updates currently running thread
	BL G8RTOS_Scheduler

	;Pops LR
	pop {LR}

	;Loads new SP
	ldr r4, RunningPtr
	ldr r5, [r4,#0]
	ldr sp, [r5,#0]

	;Restores resgisters
	pop {R4-R11}

	;Enables interrupts
	CPSIE I

	;Returns
	bx LR

	.endasmfunc
	
	; end of the asm file
	.align
	.end
