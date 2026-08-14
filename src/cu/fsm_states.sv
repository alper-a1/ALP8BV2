`ifndef FSM_STATES_SV
`define FSM_STATES_SV

package fsm_states;

  typedef enum logic [5:0] {
    S_ERROR,  // TODO: NOP FOR NOW (cause lockup)
    S_FETCH_IR,
    S_FETCH_INPC,
    S_EXEC_1CYCLE
  } state_t;

endpackage

`endif  // FSM_STATES_SV
