`ifndef ALU_DEFS_SV
`define ALU_DEFS_SV

package alu_defs;

  // public enum. single source of truth for both verilog and c++ test benches.
  typedef enum bit [3:0] {
    ALU_ADD = 4'b0000,
    ALU_ADC = 4'b0001,
    ALU_SUB = 4'b0010,
    ALU_SBC = 4'b0011,
    ALU_OR  = 4'b0100,
    ALU_AND = 4'b0101,
    ALU_XOR = 4'b0110,
    ALU_SHL = 4'b0111,
    ALU_SHR = 4'b1000,
    ALU_NOT = 4'b1001,
    ALU_INC = 4'b1010,
    ALU_DEC = 4'b1011
  } alu_op_t  /*verilator public*/;

endpackage

`endif  // ALU_DEFS_SV

