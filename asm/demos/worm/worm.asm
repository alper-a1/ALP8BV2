.NAME worm program
.CLOCK 500

; Worm program, slithers through memory

; Each worm copies its instructions forward to its clone while simultaneously killing its own grandparent.
; As the current worm is about to die, it performs 'surgery' on its clone to update the jump addresses of its
; clone such that the clone can loop on its own (since the ISA does not have PC relative jumps, this must be done)


; bootstrapping for the first ever worm
; this is overwritten once the worm wraps around 0xFF and restarts
LDI R0 4        ; bootstrap sequence is 4 bytes long, so the first worm begins at offset 4.
LDI R2 32       ; 28 byte initial worm + 4 byte bootstrap

; what needs to be set at the start of the loop:
; R0: current worm first instruction
; R2: boundary counter to last instruction + 1

; copy clone / kill grandparent loop
loop:
LDI R1 28         ; 0,1    load the static worm length
SUB R0 R1         ; 2      R0 now points to grandparent
XOR R3 R3         ; 3      clear R3 to 0x00
STM R3 R0         ; 4      erase grandparent byte
ADD R0 R1         ; 5      R0 is restored to point at parent
MOV R3 R0         ; 6      begin synthesizing head pointer
ADD R3 R1         ; 7      R3 now points to clone
LDM R1 R0         ; 8      R1 is now the instruction data
STM R1 R3         ; 9      write instruction to clone
INC R0            ; 10     advance tail pointer
CBEQ R2 R0 end    ; 11,12  if tail != end, continue loop
JMP loop          ; 13,14

; once we here, the program has naturally copied itself fully & killed its grandparent
; R0 is resting at the address of the first instruction of the next worm
; need to change loop imm8s and recalculate R2 (boundary check)

end:
MOV R3 R0               ; 15     dont touch R0
LDI R1 14               ; 16,17  14 bytes needed to get from instruction 0 of worm to imm8 of JMP loop
ADD R3 R1               ; 18     get R3 to point at the imm8 value of JMP loop instruction
STM R0 R3               ; 19     JMP loop: is to point at address of new loop: label (start of worm)
MOV R2 R3               ; 20
INC R2                  ; 21     R2 now points at end: label
DEC R3                  ; 22
DEC R3                  ; 23     R3 now points at imm8 of CBEQ end instruction
STM R2 R3               ; 24     CBEQ end is to point at address of new end: label
LDI R1 13               ; 25,26  13 bytes needed to get from end: to final instruction + 1
ADD R2 R1               ; 27     R2 now correctly bootstrapped for next worm

; from here on, the program continues at the beginning again through the cloned code (new worm)
