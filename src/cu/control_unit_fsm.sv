`include "control_signals.sv"
`include "instruction_types.sv"
`include "fsm_states.sv"

module control_unit_fsm
  import control_signals::*;
  import instruction_types::*;
  import fsm_states::*;
(
    input logic clk,
    input logic rst_n, // synchronous reset (back to S_FETCH_IR)


    input logic zero_flag,
    input logic carry_flag,

    input instruction_t instr,
    output control_signals_t ctrl
);

  state_t state, next_state;

  // pre decoder helper for instruction type (instr union tracking)
  instr_format_t instr_fmt;

  always_comb begin : PRE_DECODER
    // is it a DUAL register instruction?
    if (instr.dual.opcode != 4'b0000 && instr.dual.opcode != 4'b0001 && instr.dual.opcode != 4'b0100) begin
      instr_fmt = FMT_DUAL;
    end  // is it a SINGLE register instruction?
    else if (instr.single.opcode != 6'b0000_00 && instr.single.opcode != 6'b0100_00) begin
      instr_fmt = FMT_SINGLE;
    end  // it must be a NO OPERAND instruction
    else begin
      instr_fmt = FMT_NONE;
    end
  end


  // next state / reset
  // ---------------------------------------------------------------------------
  always_ff @(posedge clk) begin : STATE_TRANSITION
    if (!rst_n) begin
      // TODO: error state eventually cleans everything and restarts (ram/pc reset)
      // for now, error state just bricks the cpu
      state <= S_FETCH_IR;
    end else begin
      state <= next_state;
    end
  end

  // next state update
  // ---------------------------------------------------------------------------
  always_comb begin : NEXT_STATE_UPDATE
    case (state)
      // FETCH STAGE
      // -----------
      S_FETCH_IR: next_state = S_FETCH_INPC;

      // DECODING (baked into the end of fetch cycle)
      // default move to error state (not implemented yet)
      S_FETCH_INPC: begin
        case (instr_fmt)

          FMT_NONE: begin
            case (instr.none.opcode)
              OP_NOP: next_state = S_FETCH_IR;
              OP_RST, OP_CLC, OP_SEC: next_state = S_EXEC_1CYCLE;
              // TODO: JMP
              default: next_state = S_ERROR;
            endcase
          end

          FMT_SINGLE: begin
            case (instr.single.opcode)
              OP_RNG:  next_state = S_EXEC_1CYCLE;
              default: next_state = S_ERROR;
            endcase
          end

          FMT_DUAL: begin
            case (instr.dual.opcode)
              OP_MOV:  next_state = S_EXEC_1CYCLE;
              default: next_state = S_ERROR;
            endcase
          end

          default: next_state = S_ERROR;
        endcase
      end

      // EXECUTE STAGE:
      // --------------
      S_EXEC_1CYCLE: next_state = S_FETCH_IR;

      default: next_state = S_ERROR;
    endcase
  end

  // output
  // ---------------------------------------------------------------------------
  always_comb begin : OUTPUT
    // defaults output to zeros (to be overridden in logic)
    ctrl = '0;

    case (state)
      S_FETCH_IR: begin
        ctrl.src_sel = BUS_SRC_RAM;
        ctrl.dst_sel = BUS_DST_IR;
        // addr bus mux already defaulted to PC 
      end
      S_FETCH_INPC: begin
        ctrl.pc_inc = 1'b1;
      end

      // single cycle instructions
      // OP_RST, OP_CLC, OP_SEC, OP_RNG, OP_MOV
      S_EXEC_1CYCLE: begin
        case (instr_fmt)
          FMT_NONE: begin
            case (instr.none.opcode)
              OP_RST:  ctrl.pc_rst = 1'b1;
              OP_CLC: begin
                ctrl.flgs_overwrite = 1'b1;
                ctrl.flgs_ow_value = 1'b0;
                ctrl.flgs_c_we = 1'b1;
              end
              OP_SEC: begin
                ctrl.flgs_overwrite = 1'b1;
                ctrl.flgs_ow_value = 1'b1;
                ctrl.flgs_c_we = 1'b1;
              end
              default: ;
            endcase
          end

          FMT_DUAL: begin
            case (instr.dual.opcode)
              // single cycle move (copy)
              OP_MOV: begin
                ctrl.dst_sel = BUS_DST_REG_A;
                ctrl.src_sel = BUS_SRC_REG_B;
              end
              default: ;
            endcase
          end

          FMT_SINGLE: begin
            case (instr.single.opcode)
              // single cycle rng value latch
              OP_RNG: begin
                ctrl.dst_sel = BUS_DST_REG_B;
                ctrl.src_sel = BUS_SRC_LFSR;
              end
              default: ;
            endcase
          end

          default: ;  // empty; zeroed at top
        endcase
      end




      default: ;  // empty; zeroed at top
    endcase
  end

endmodule
