`ifndef FSM_STATES_SV
`define FSM_STATES_SV

package fsm_states;

  typedef enum logic [5:0] {
    S_ERROR,  // TODO: NOP FOR NOW (cause lockup)
    S_FETCH_IR,  // entry point
    S_FETCH_INPC,

    S_EXEC_1CYCLE,  // instructions that exec in one clock cycle

    S_EXEC_2R_MATH_TMP,  // two reg math write to alutmp
    S_EXEC_2R_MATH_RES,  // two reg math latch alu result
    S_EXEC_2R_MATH_WB,   // two reg math writeback result

    S_EXEC_1R_MATH_RES,  // single reg math put operand A on bus & latch alures
    S_EXEC_1R_MATH_WB,   // single reg math writeback

    S_EXEC_MAR_RB_L,  // latch rb into MAR (used by LDM)
    S_EXEC_RD_RAM,    // read from ram [into ra] (LDM)
    S_EXEC_MAR_RB_S,  // latch rb into MAR (used by STM)
    S_EXEC_WR_RAM,    // write to ram [from ra] (STM)

    S_EXEC_LDI_LM,  // LDI load imm8 from mem
    S_EXEC_LDI_IP,  // LDI inc pc past imm8 and onto next instruction

    S_EXEC_1R_CB_C1,  // 1reg compare to zero get flags from operand
    S_EXEC_1R_CB_C2,  // 1reg compare to zero take branch if conditon

    S_EXEC_2R_CB_TMP,  // 2reg compare and branch latch tmp
    S_EXEC_2R_CB_RES,  // 2reg compare and branch get flag results
    S_EXEC_2R_CB_BRC   // 2reg compare and branch take branch if condition

  } state_t;

endpackage

`endif  // FSM_STATES_SV
