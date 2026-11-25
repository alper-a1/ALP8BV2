# =============================================================================
# ALP8BV2 — Gowin build script (run via: gw_sh build.tcl, or `make` in fpga/)
#
# Flow: create_project -> add_file -> set_option -> run all
#       (synthesis -> place&route -> bitstream -> reports)
#
# Output: gowin_out/<TOP>/impl/pnr/<TOP>.fs   (+ .tr timing, .rpt.txt reports)
# =============================================================================

# --- project knobs ------------------------------------------------------------
set TOP     top_blink
set DEVICE  GW1NR-LV9QN88PC6/I5
set DEVVER  C

# Anchor all source paths to THIS script's location, so the build works no
# matter which directory gw_sh is invoked from (create_project changes the
# cwd to the project dir, which breaks bare relative paths — hence anchoring).
set SCRIPT_DIR [file dirname [file normalize [info script]]]
set ROOT       [file normalize $SCRIPT_DIR/..]

# --- source list: grow this as milestones land --------------------------------
# Paths are relative to the repo root (short & readable); the script anchors
# them below — this is how src/*.sv files will join for M1, e.g. just:
#   src/cpu_top.sv
set SOURCES [list \
    fpga/top/top_blink.v \
]
set CONSTRAINTS [list \
    fpga/constraints/tangnano9k.cst \
    fpga/constraints/tangnano9k.sdc \
]

# --- run ----------------------------------------------------------------------
create_project -name $TOP -dir $SCRIPT_DIR/gowin_out -pn $DEVICE -device_version $DEVVER -force

foreach f $SOURCES      { add_file [file join $ROOT $f] }
foreach f $CONSTRAINTS  { add_file -type [expr {[file extension $f] eq ".cst" ? "cst" : "sdc"}] [file join $ROOT $f] }

set_option -top_module $TOP
set_option -verilog_std sysv2017

run all
