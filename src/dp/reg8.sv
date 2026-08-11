// 8bit register module, async read with sync write on posedge clk

module reg8 (
    input logic clk,
    input logic [7:0] d_in,
    input logic we,
    output logic [7:0] d_out
);

  // public for testing & sim visualisation
  logic [7:0] val  /*verilator public*/;

  always_ff @(posedge clk) begin
    if (we) begin
      val <= d_in;
    end
  end

  assign d_out = val;

endmodule
