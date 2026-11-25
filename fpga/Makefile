# ALP8BV2 — FPGA build (Gowin gw_sh CLI + openFPGALoader)
#
#   make          syn + pnr + bitstream
#   make flash    load bitstream into FPGA SRAM (volatile, survives until power-off)
#   make flash-spi program the onboard SPI flash (persistent)  [needs confirmation]
#   make open     open the resulting project in the Gowin GUI (inspect reports/pins)
#   make clean    remove build output

GOWIN_OUT := gowin_out
TOP       := top_blink
BITSTREAM := $(GOWIN_OUT)/$(TOP)/impl/pnr/$(TOP).fs

.PHONY: all flash flash-spi open clean

all:
	gw_sh build.tcl

flash: $(BITSTREAM)
	openFPGALoader -b tangnano9k $(BITSTREAM)

flash-spi: $(BITSTREAM)
	openFPGALoader -b tangnano9k -m jtag -o 2 $(BITSTREAM)

open:
	gw_ide $(GOWIN_OUT)/$(TOP)/$(TOP).gprj &

clean:
	rm -rf $(GOWIN_OUT)
