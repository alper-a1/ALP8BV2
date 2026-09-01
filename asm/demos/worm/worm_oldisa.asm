; Worm program, slithers through memory

; Each worm copies its instructions forward to its clone while simultaneously killing its own grandparent.
; As the current worm is about to die, it performs 'surgery' on its clone to update the jump addresses of its
; clone such that the clone can loop on its own (since the ISA does not have PC relative jumps, this must be done)


; bootstrapping for the first ever worm 
; this is overwritten once the worm wraps around 0xFF and restarts
LDI R0 4        ; bootstrap sequence is 4 bytes long, so first worm begins after 6 bytes.
LDI R2 37       ; 33 byte initial worm + 4 byte bootstrap 

; what needs to be set at the start of the loop:
; R0: current worm first instruction
; R2: boundary counter to last instruction + 1

; copy clone / kill grandparent loop 
loop: 
LDI R1 33         ; 0,1    load the static worm length
SUB R0 R1         ; 2      R0 now points to grandparent
XOR R3 R3         ; 3      clear R3 to 0x00
STM R3 R0         ; 4      erase grandparent byte
CLC               ; 5      
ADC R0 R1         ; 6      R0 is restored to point at parent
MOV R3 R0         ; 7      begin synthesizing head pointer
CLC               ; 8      
ADC R3 R1         ; 9      R3 now points to clone
LDM R1 R0         ; 10     R1 is now the instruction data
STM R1 R3         ; 11     write instruction to clone
INC R0            ; 12     advance tail pointer
CMP R2 R0         ; 13     if tail != end, continue loop (pseudo JNE instruction)
JEQ end           ; 14,15  
JMP loop          ; 16,17

; once we here, the program has naturally copied itself fully & killed its grandparent
; R0 is resting at the address of the first instruction of the next worm
; need to change loop imm8s and recalculate R2 (boundary check)

end:
MOV R3 R0               ; 18     dont touch R0
LDI R1 17               ; 19,20  17 bytes needed to get from instruction 0 of worm to imm8 of JMP loop
CLC                     ; 21 
ADC R3 R1               ; 22     get R3 to point at the imm8 value of JMP loop instruction
STM R0 R3               ; 23     JMP loop: is to point at address of new loop: label (start of worm)
MOV R2 R3               ; 24    
INC R2                  ; 25     R2 now points at end: label
DEC R3                  ; 26
DEC R3                  ; 27     R3 now points at imm8 of JEQ end instruction
STM R2 R3               ; 28     JEQ end is to point at address of new end: label
LDI R1 15               ; 29,30  15 bytes needed to get from end: to final instruction + 1 ()
CLC                     ; 31
ADC R2 R1               ; 32     R2 now correctly bootstrapped for next worm  

; from here on, the program continues at the beginning again through the cloned code (new worm)