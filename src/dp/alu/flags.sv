
// 2bit flag register module
// sync write on posedge clk
// always outputs flags
module flags (
    input logic clk,

    input logic rst_n,  // hardware reset active low (async)

    input  logic z_in,
    input  logic z_we,
    output logic z_out,

    input  logic c_in,
    input  logic c_we,
    output logic c_out
);

  // public for testing & sim visualisation
  logic zero_flag  /*verilator public*/;
  logic carry_flag  /*verilator public*/;

  always_ff @(posedge clk or negedge rst_n) begin

    if (!rst_n) begin
      carry_flag <= 1'b0;
      zero_flag  <= 1'b0;
    end else begin
      if (c_we) begin
        carry_flag <= c_in;
      end

      if (z_we) begin
        zero_flag <= z_in;
      end
    end
  end

  assign c_out = carry_flag;
  assign z_out = zero_flag;

endmodule
