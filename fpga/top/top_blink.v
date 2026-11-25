// ALP8BV2 — M0 placeholder top: heartbeat LEDs from the 27 MHz oscillator.
// Purpose: prove fpga/ build -> bitstream -> flash works from this repo.
// This is a build-system placeholder — replace freely.

module top_blink (
    input  wire       clk,    // 27 MHz oscillator, pin 52
    input  wire       rst_n,  // user button K1, pin 4, active low
    input  wire       k2,     // user button K2, pin 3, active low
    output wire [5:0] led
);
    logic [26:0] counter = 0;

    always_ff @(posedge clk) begin
        if (!rst_n)
            counter <= 0;
        else
            counter <= counter + 1;
    end

    // four visible blink rates on led[0..3]; K1 -> led[4], K2 -> led[5]
    logic [1:0] btn_sync;
    always_ff @(posedge clk) btn_sync <= {k2, rst_n};   // 1-stage filters (buttons are pinned low when released)

    assign led = {~btn_sync[1], ~btn_sync[0], counter[23], counter[22], counter[21], counter[20]};
endmodule
