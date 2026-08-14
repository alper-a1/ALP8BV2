`ifndef CONTROL_SIGNALS_SV
`define CONTROL_SIGNALS_SV

`include "../dp/alu/alu_defs.sv"

package control_signals;
  import alu_defs::*;


  typedef enum logic [2:0] {
    BUS_SRC_NONE,   // high z (no verilator support, so 0x00 is emulated high z)
    BUS_SRC_REG_A,
    BUS_SRC_REG_B,
    BUS_SRC_RAM,
    BUS_SRC_ALU,
    BUS_SRC_LFSR
  } bus_src_t;

  typedef enum logic [3:0] {
    BUS_DST_NONE,    // high z (no verilator support, so 0x00 is emulated high z)
    BUS_DST_REG_A,
    BUS_DST_REG_B,
    BUS_DST_RAM,
    BUS_DST_ALUTMP,
    BUS_DST_PC,
    BUS_DST_IR,
    BUS_DST_MAR
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
