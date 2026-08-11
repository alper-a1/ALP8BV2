
// Linear Feedback Shift Register for prng numbers
// only outputs.
module lfsr_rng (
    input  logic       clk,
    input  logic       rst_n,    // hardware active low reset (async)
    output logic [7:0] rand_out
);

  // X^8 + X^6 + X^5 + X^4 + 1 feedback polynomial taps
  logic feedback = rand_out[7] ^ rand_out[5] ^ rand_out[4] ^ rand_out[3];

  always_ff @(posedge clk or posedge rst_n) begin : blockName
    if (!rst_n) begin
      rand_out <= 8'hAA;
    end else begin
      rand_out <= {rand_out[6:0], feedback};
    end
  end

endmodule
