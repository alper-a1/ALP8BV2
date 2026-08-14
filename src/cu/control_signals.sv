`ifndef CONTROL_SIGNALS_SV
`define CONTROL_SIGNALS_SV

`include "alu_defs.sv"

package control_signals;
  import alu_defs::*;


  // TODO: refactor the GPR0/1/2/3 into GPRA/GPRB , let datapath do the decoding
  // this way its cleaner when the FSM is outputting the signals - datapath can just take register address directly
  // from the ir_out ( can also make the bus_dst_t thus one bit smaller)
  typedef enum logic [2:0] {
    BUS_SRC_NONE = 3'd0,  // high z (no verilator support, so 0x00 is emulated high z)
    BUS_SRC_GPR0 = 3'd1,
    BUS_SRC_GPR1 = 3'd2,
    BUS_SRC_GPR2 = 3'd3,
    BUS_SRC_GPR3 = 3'd4,
    BUS_SRC_RAM  = 3'd5,
    BUS_SRC_ALU  = 3'd6,
    BUS_SRC_LFSR = 3'd7
  } bus_src_t;

  typedef enum logic [3:0] {
    BUS_DST_NONE   = 4'd0,  // high z (no verilator support, so 0x00 is emulated high z)
    BUS_DST_GPR0   = 4'd1,
    BUS_DST_GPR1   = 4'd2,
    BUS_DST_GPR2   = 4'd3,
    BUS_DST_GPR3   = 4'd4,
    BUS_DST_RAM    = 4'd5,
    BUS_DST_ALUTMP = 4'd6,
    BUS_DST_PC     = 4'd7,
    BUS_DST_IR     = 4'd8,
    BUS_DST_MAR    = 4'd9
  } bus_dst_t;


  // every control signal output from the fsm
  typedef struct packed {
    // bus src/dest muxs
    bus_src_t src_sel;
    bus_dst_t dst_sel;

    // alu
    alu_op_t aluop;
    logic alutmp_zero;  // 2:1 mux control for alu B input (0: TMP d_out / 1: hardwired 8'b0)
    logic flgs_overwrite; // 2:1 mux control for carry flag overwrite (0: alu input / 1: accept ow_value if c_we)
    logic flgs_ow_value;  // value to overwrite with (must be paired with c_we & overwrite = 1) 
    logic flgs_c_we;
    logic flgs_z_we;

    // additional control
    logic pc_inc;    // program counter internal increment
    logic pc_rst;    // software reset to 0x00 (RST instruction)
    logic addr_sel;  // 2:1 mux control for ram addressing (0: PC / 1: MAR)

  } control_signals_t;


endpackage

`endif  // CONTROL_SIGNALS_SV
