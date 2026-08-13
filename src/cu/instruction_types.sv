`ifndef INSTRUCTION_TYPES_SV
`define INSTRUCTION_TYPES_SV

package instruction_types;

  typedef struct packed {logic [7:0] opcode;} instr_no_reg_t;

  typedef struct packed {
    logic [5:0] opcode;
    logic [1:0] reg_b;
  } instr_single_reg_t;

  typedef struct packed {
    logic [3:0] opcode;
    logic [1:0] reg_a;
    logic [1:0] reg_b;
  } instr_dual_reg_t;

  typedef union packed {
    instr_no_reg_t none;
    instr_single_reg_t single;
    instr_dual_reg_t dual;
  } instruction_t;

endpackage

`endif  // INSTRUCTION_TYPES_SV
