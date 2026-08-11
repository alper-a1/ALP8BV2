`include "control_signals.sv"


module control_unit_fsm
  import control_signals::*;
(
    input logic clk,

    input logic zero_flag,
    input logic carry_flag,

    input logic [7:0] opcode,
    output control_signals_t ctrl
);

endmodule
