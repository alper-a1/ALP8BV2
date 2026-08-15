// 8bit program counter module, fixed output
// sync write from bus on posedge clk (take branch address)
// sync reset on posedge clk
// sync increment on posedge clk

module pc (
    input logic clk,
    input logic [7:0] d_in,
    input logic we,
    input logic inc,
    input logic sw_rst,  // software reset active high (synchronous)
    input logic rst_n,  // hardware reset active low (async)
    output logic [7:0] d_out
);

  // public for testing & sim visualisation
  logic [7:0] val  /*verilator public*/;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      val <= 8'b0;
    end else if (sw_rst) begin
      val <= 8'b0;
    end else if (inc) begin
      val <= val + 8'd1;
    end else if (we) begin
      val <= d_in;
    end

    if ($countones({sw_rst, we, inc}) > 1) begin : CHK_SINGLE_DRIVER
      $error("pc: rst/inc/we asserted simultaneously");
    end

  end

  assign d_out = val;

endmodule
