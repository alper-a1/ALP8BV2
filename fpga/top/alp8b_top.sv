// =============================================================================
// ALP8BV2 — M1 top: cpu_top on silicon, clock-enable style.
//
// You (the human) own this file from here; the TODOs mark your design seams.
// Skeleton provided by step-0 — verified structure: cpu_top's only ports are
// clk/rst_n, so observation = hierarchical probes (legal for synthesis).
// =============================================================================

module alp8b_top
  import alp8b_file::*;
(
    input  wire        clk,  // 27 MHz oscillator (pin 52)
    input  wire        k1,   // button K1 (pin 4), active low
    input  wire        k2,   // button K2 (pin 3), active low
    output logic [5:0] led
);
  // ---------------------------------------------------------------------------
  // clock enable: ONE clock domain (27 MHz); CPU ticks only when en pulses.
  // CLK_DIV = 27_000_000 / .CLOCK, i.e. exactly the .alp8b `clock_div` field
  // (little endian uint16, offset 454). worm .CLOCK 500 -> 54000.
  // ---------------------------------------------------------------------------
  // CLK_DIV read straight from the .alp8b header (LE uint16 @ CLOCK_DIV_OFFSET):
  // worm .CLOCK 500 -> 54000. Assembler precalculates it (27 MHz / .CLOCK).
  localparam logic [15:0] CLK_DIV = {IMAGE[CLOCK_DIV_OFFSET + 1], IMAGE[CLOCK_DIV_OFFSET]};

  logic [15:0] div_cnt;
  logic        ce;

  always_ff @(posedge clk) begin
    if (div_cnt == CLK_DIV - 16'd1) div_cnt <= 16'd0;
    else div_cnt <= div_cnt + 16'd1;
  end
  assign ce = (div_cnt == CLK_DIV - 16'd1);  // exactly one cycle every CLK_DIV (cpu clock)

  // ---------------------------------------------------------------------------
  // program load: ROM -> RAM, then release reset. K1 retriggers a reload.
  // One counter is the single source of truth; loader outputs are combinational
  // (ram's own register samples them on the same edge — no pipeline to skew).
  // ---------------------------------------------------------------------------
  /* verilator lint_off PROCASSINIT */  // = 0 is the FPGA FF power-on init (via GSR, set in bitstream)
  logic [7:0] ld_cnt = 0;
  logic       ld_done = 0;
  /* verilator lint_on PROCASSINIT */

  always_ff @(posedge clk) begin
    if (!k1) begin  // button = reboot: re-run the loader
      ld_cnt  <= 8'd0;
      ld_done <= 1'b0;
    end else if (!ld_done) begin
      ld_cnt <= ld_cnt + 8'd1;
      if (ld_cnt == 8'hFF) ld_done <= 1'b1;
    end
  end

  logic       ld_we;
  logic [7:0] ld_addr;
  logic [7:0] ld_d_in;
  logic       rst_n;

  assign ld_we   = ~ld_done;
  assign ld_addr = ld_cnt;
  assign ld_d_in = IMAGE[PROG_OFFSET+int'(ld_cnt)];

  // CPU sleeps while loading. k1 idles high (pull-up): pressed -> low -> reset + reload.
  assign rst_n   = ld_done & k1;


  // ---------------------------------------------------------------------------
  // cpu itself
  // ---------------------------------------------------------------------------
  cpu_top dut (
      .clk(clk),
      .rst_n(rst_n),
      .en(ce),
      //bootloader
      .ld_we(ld_we),
      .ld_addr(ld_addr),
      .ld_d_in(ld_d_in)
  );

  // ---------------------------------------------------------------------------
  // observation: M1 = watch the worm crawl. led[0..3] = CPU internals via
  // hierarchical probes (legal for synthesis — same trick /*verilator public*/
  // gives the C++ sim). Prescale/slow them down or repin as you like.
  // ---------------------------------------------------------------------------
  always_ff @(posedge clk) begin
    led[0] <= dut.dp.u_pc.val[0];
    led[1] <= dut.dp.u_pc.val[3];
    led[2] <= dut.dp.u_pc.val[7];
    led[3] <= dut.dp.u_ir.val[0];
    led[4] <= ~k1;
    led[5] <= ~k2;
  end

endmodule
