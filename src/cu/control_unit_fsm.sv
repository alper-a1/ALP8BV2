`include "control_signals.sv"
`include "instruction_types.sv"

module control_unit_fsm
  import control_signals::*;
  import instruction_types::*;
(
    input logic clk,

    input logic zero_flag,
    input logic carry_flag,

    input instruction_t instr,
    output control_signals_t ctrl
);


endmodule
