// special variant of reg8 (async reset to 0x00)
// async read with sync write on posedge clk

module reg8ar (
    input logic clk,
    input logic [7:0] d_in,
    input logic oe,
    input logic we,
    input logic areset,
    output logic [7:0] d_out
);

  // public for testing & sim visualisation
  logic [7:0] val  /*verilator public*/;

  always_ff @(posedge clk or posedge areset) begin
    if (areset) begin
      val <= 8'd0;
    end else if (we) begin
      val <= d_in;
    end

  end

  assign d_out = oe ? val : 8'b0;

endmodule
