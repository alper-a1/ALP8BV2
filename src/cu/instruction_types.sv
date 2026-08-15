`ifndef INSTRUCTION_TYPES_SV
`define INSTRUCTION_TYPES_SV

package instruction_types;

  // helper enum for the decoder phase,
  // used to track which of the instr_* enum union is currently active
  typedef enum logic [1:0] {
    FMT_NONE,
    FMT_SINGLE,
    FMT_DUAL
  } instr_format_t;

  // opcodes for each instruction type

  typedef enum logic [7:0] {
    OP_NOP = 8'b0000_0000,
    OP_RST = 8'b0000_0001,
    OP_CLC = 8'b0000_0010,
    OP_SEC = 8'b0000_0011,
    OP_JMP = 8'b0100_0000,
    OP_JC  = 8'b0100_0001,
    OP_JNC = 8'b0100_0010
  } op_no_reg_t;

  typedef enum logic [5:0] {
    OP_ROR  = 6'b0000_01,
    OP_SHR  = 6'b0000_10,
    OP_NOT  = 6'b0000_11,
    OP_INC  = 6'b0001_00,
    OP_DEC  = 6'b0001_01,
    OP_LDI  = 6'b0001_10,
    OP_RNG  = 6'b0001_11,
    OP_JMPR = 6'b0100_01,
    OP_CBZ  = 6'b0100_10,
    OP_CBNZ = 6'b0100_11
  } op_single_reg_t;

  typedef enum logic [3:0] {
    OP_SUB  = 4'b0010,
    OP_SBC  = 4'b0011,
    OP_CBEQ = 4'b0101,
    OP_CBNE = 4'b0110,
    OP_CBLT = 4'b0111,
    OP_ADD  = 4'b1000,
    OP_ADC  = 4'b1001,
    OP_OR   = 4'b1010,
    OP_AND  = 4'b1011,
    OP_XOR  = 4'b1100,
    OP_LDM  = 4'b1101,
    OP_STM  = 4'b1110,
    OP_MOV  = 4'b1111
  } op_dual_reg_t;

  // instruction types (ir register layout)

  typedef struct packed {op_no_reg_t opcode;} instr_no_reg_t;

  typedef struct packed {
    op_single_reg_t opcode;
    logic [1:0] reg_b;
  } instr_single_reg_t;

  typedef struct packed {
    op_dual_reg_t opcode;
    logic [1:0]   reg_a;
    logic [1:0]   reg_b;
  } instr_dual_reg_t;

  typedef union packed {
    instr_no_reg_t none;
    instr_single_reg_t single;
    instr_dual_reg_t dual;
  } instruction_t;


endpackage

`endif  // INSTRUCTION_TYPES_SV
