

// async output ram, writes on posedge of clk.
module ram (
    input logic clk,
    input logic [7:0] addr,
    input logic we,
    input logic oe,
    input logic [7:0] d_in,
    output logic [7:0] d_out
);

  // internal memory; public so c++ accessible 
  // required such that programs can be loaded via c++
  logic [7:0] mem[255:0]  /*verilator public*/;

  always_ff @(posedge clk) begin
    if (we) begin
      mem[addr] <= d_in;
    end
  end

  assign d_out = oe ? mem[addr] : 8'b0;

endmodule
