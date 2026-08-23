; built-in macros
; contains pseudo instructions and other core patterns

%MACRO HALT
halt:
JMP halt
%ENDMACRO

%MACRO CLR RA
XOR RA RA
%ENDMACRO

%MACRO CBGT RA RB lbl
CBLT RB RA lbl
%ENDMACRO

%MACRO SHL RA
ADD RA RA
%ENDMACRO