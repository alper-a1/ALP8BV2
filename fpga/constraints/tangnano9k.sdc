// ALP8BV2 — timing constraints (.sdc)
// The ONLY real constraint: the 27 MHz oscillator on pin 52.
// PLL/CLKDIV-derived clocks are inferred by the tools from the IP parameters,
// so nothing else goes here (same minimal style as the Sipeed samples).
create_clock -name clk_osc -period 37.037 -waveform {0 18.518} [get_ports {clk}]
