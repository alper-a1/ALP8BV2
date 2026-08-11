`include "alu_defs.svh"

module alu
  import alu_defs::*;
(
    input logic [7:0] a,  // bus input
    input logic [7:0] b,  // temp input
    input alu_op_t opcode,
    input logic cin_flag,
    output logic cout_flag,
    output logic zero_flag,
    output logic [7:0] result
);

  always_comb begin
    // defaults; zero flag evaluated at bottom
    cout_flag = 1'b0;
    result = 8'b0;

    case (opcode)
      ALU_ADD: {cout_flag, result} = a + b;

      ALU_ADC: {cout_flag, result} = a + b + cin_flag;

      ALU_SUB: {cout_flag, result} = a - b;  // cout == 1 means borrow occured.

      ALU_SBC: {cout_flag, result} = a - b - cin_flag;

      ALU_OR: result = a | b;

      ALU_AND: result = a & b;

      ALU_XOR: result = a ^ b;

      ALU_SHL: begin
        result = a << 8'd1;
        cout_flag = a[7];
      end

      ALU_SHR: begin
        result = a >> 8'd1;
        cout_flag = a[0];
      end

      ALU_NOT: result = ~a;

      ALU_INC: result = a + 8'd1;

      ALU_DEC: result = a - 8'd1;

      default: ;  // defaults are handled at the top
    endcase

    // only compare and branch instructions will ever need this (CBxx type)
    zero_flag = (result == 8'b0);

  end

endmodule
