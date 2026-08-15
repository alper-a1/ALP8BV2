`include "control_signals.sv"
`include "instruction_types.sv"

module cpu_top
  import control_signals::control_signals_t;
  import instruction_types::instruction_t;
(
    input logic clk,
    input logic rst_n  // syncronous
);
  // internal wiring between fsm & datapath
  logic zf, cf;
  control_signals_t ctrl;
  instruction_t instr;

  control_unit_fsm fsm (
      .clk(clk),
      .rst_n(rst_n),
      .zero_flag(zf),
      .carry_flag(cf),
      .instr(instr),
      .ctrl(ctrl)
  );

  cpu_datapath dp (
      .clk(clk),
      .rst_n(rst_n),
      .zero_flag(zf),
      .carry_flag(cf),
      .ctrl(ctrl),
      .ir_out(instr)
  );

endmodule
