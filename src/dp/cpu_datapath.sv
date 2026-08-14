`include "../cu/control_signals.sv"

module cpu_datapath
  import control_signals::*;
(
    input logic clk,
    input logic rst_n,

    input control_signals_t ctrl,

    output logic zero_flag,
    output logic carry_flag,
    output logic [7:0] ir  // IR output (opcode)

);
  // internal wiring
  // ---------------
  logic [7:0] main_bus;

  // input of ram addr
  logic [7:0] addr_bus;

  // input of alu b
  logic [7:0] alu_bin;
  logic flags_carry_input;

  // component outputs
  // -----------------
  // gprs
  logic [7:0] gpr0_out, gpr1_out, gpr2_out, gpr3_out;

  // ram & mar
  logic [7:0] ram_out, mar_out;

  // alu
  logic [7:0] alures_out, alutmp_out, alu_out;
  logic alu_carry_out, alu_zero_out;

  // other
  logic [7:0] pc_out, lfsr_out;

  // component write enables
  // -----------------------

  // gprs
  logic gpr0_we, gpr1_we, gpr2_we, gpr3_we;

  // alu
  logic alutmp_we;

  // other
  logic mar_we, ram_we, pc_we, ir_we;


  // source decoding:
  // main bus mux
  always_comb begin
    case (ctrl.src_sel)
      BUS_SRC_GPR0: main_bus = gpr0_out;
      BUS_SRC_GPR1: main_bus = gpr1_out;
      BUS_SRC_GPR2: main_bus = gpr2_out;
      BUS_SRC_GPR3: main_bus = gpr3_out;
      BUS_SRC_RAM: main_bus = ram_out;
      BUS_SRC_ALU: main_bus = alures_out;
      BUS_SRC_LFSR: main_bus = lfsr_out;
      default: main_bus = 8'b0;  // same as BUS_SRC_NONE; verilator high-z workaround
    endcase

  end

  // addr bus mux
  assign addr_bus = ctrl.addr_sel ? mar_out : pc_out;

  // alu b mux 
  assign alu_bin = ctrl.alutmp_zero ? 8'b0 : alutmp_out;

  // carry flag write source bus (either ALU or hard fix from ctrl)
  assign flags_carry_input = ctrl.flgs_overwrite ? ctrl.flgs_ow_value : alu_carry_out;

  // destination decoding:
  assign gpr0_we = (ctrl.dst_sel == BUS_DST_GPR0);
  assign gpr1_we = (ctrl.dst_sel == BUS_DST_GPR1);
  assign gpr2_we = (ctrl.dst_sel == BUS_DST_GPR2);
  assign gpr3_we = (ctrl.dst_sel == BUS_DST_GPR3);

  assign ram_we = (ctrl.dst_sel == BUS_DST_RAM);
  assign alutmp_we = (ctrl.dst_sel == BUS_DST_ALUTMP);
  assign pc_we = (ctrl.dst_sel == BUS_DST_PC);
  assign ir_we = (ctrl.dst_sel == BUS_DST_IR);
  assign mar_we = (ctrl.dst_sel == BUS_DST_MAR);


  // components
  // ----------

  reg8 gpr0 (
      .clk(clk),
      .d_in(main_bus),
      .d_out(gpr0_out),
      .we(gpr0_we)
  );

  reg8 gpr1 (
      .clk(clk),
      .d_in(main_bus),
      .d_out(gpr1_out),
      .we(gpr1_we)
  );

  reg8 gpr2 (
      .clk(clk),
      .d_in(main_bus),
      .d_out(gpr2_out),
      .we(gpr2_we)
  );

  reg8 gpr3 (
      .clk(clk),
      .d_in(main_bus),
      .d_out(gpr3_out),
      .we(gpr3_we)
  );

  reg8 u_ir (
      .clk(clk),
      .d_in(main_bus),
      .d_out(ir),
      .we(ir_we)
  );

  reg8 mar (
      .clk(clk),
      .d_in(main_bus),
      .d_out(mar_out),
      .we(mar_we)
  );

  reg8 alutmp (
      .clk(clk),
      .d_in(main_bus),
      .d_out(alutmp_out),
      .we(alutmp_we)
  );

  reg8 alures (
      .clk(clk),
      .d_in(alu_out),
      .d_out(alures_out),
      .we(1'b1)  // fixed; alures always consumes alu output
  );

  lfsr_rng lfsr (
      .clk(clk),
      .rst_n(rst_n),
      .rand_out(lfsr_out)
  );

  ram u_ram (
      .clk(clk),
      .d_in(main_bus),
      .d_out(ram_out),
      .we(ram_we),
      .addr(addr_bus)
  );

  pc u_pc (
      .clk(clk),
      .rst_n(rst_n),
      .d_in(main_bus),
      .we(pc_we),
      .d_out(pc_out),
      .sw_rst(ctrl.pc_rst),
      .inc(ctrl.pc_inc)
  );

  flags u_flags (
      .clk  (clk),
      .rst_n(rst_n),
      .z_out(zero_flag),
      .z_in (alu_zero_out),
      .z_we (ctrl.flgs_z_we),
      .c_out(carry_flag),
      .c_in (flags_carry_input),
      .c_we (ctrl.flgs_c_we)
  );

  alu u_alu (
      .a(main_bus),
      .b(alu_bin),
      .opcode(ctrl.aluop),
      .cin_flag(carry_flag),
      .cout_flag(alu_carry_out),
      .zero_flag(alu_zero_out),
      .result(alu_out)
  );

endmodule
