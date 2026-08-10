module top (
    input wire clk,
    output reg led
);
    initial led = 0;

    always @(posedge clk) begin
        led <= ~led;
    end

endmodule
