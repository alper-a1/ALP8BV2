.NAME test program 1
.CLOCK 10

; test of a simple program
LDI R2 0xFF  ; Set memory pointer to FF
LDI R0 0   ; Initial value

loop:
STM R0 R2    ; Store R0 into memory at FF
INC R0       ; Increment R0 by 1 (will wrap 255 -> 0 naturally in 8-bit)
JMP loop     



@INI 0xF0 0xb3