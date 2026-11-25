

// async output ram, writes on posedge of clk.
module ram (
    input logic clk,
    input logic cpu_we,
    input logic [7:0] cpu_addr,
    input logic [7:0] cpu_d_in,

    // bootloader access
    input logic [7:0] ld_addr,
    input logic ld_we,
    input logic [7:0] ld_d_in,

    output logic [7:0] d_out
);

  // internal memory; public so c++ accessible
  // required such that programs can be loaded via c++
  logic [7:0] mem[256]  /*verilator public*/;

  logic we;
  logic [7:0] addr;
  logic [7:0] d_in;

  assign we   = cpu_we | ld_we;
  assign addr = ld_we ? ld_addr : cpu_addr;
  assign d_in = ld_we ? ld_d_in : cpu_d_in;

  always_ff @(posedge clk) begin
    if (we) begin
      mem[cpu_addr] <= d_in;
    end
  end

  assign d_out = mem[addr];

endmodule
