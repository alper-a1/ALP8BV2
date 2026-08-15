`include "control_signals.sv"
`include "instruction_types.sv"
`include "fsm_states.sv"
`include "../dp/alu/alu_defs.sv"

module control_unit_fsm
  import control_signals::*;
  import instruction_types::*;
  import fsm_states::*;
  import alu_defs::*;
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
              // 1reg alu math instructions goto the 2cycle starting at res latch 
              OP_ROR, OP_SHR, OP_NOT, OP_INC, OP_DEC: next_state = S_EXEC_1R_MATH_RES;

              // ldi / rng / jmpr / cbz+cbnz all follow different paths
              OP_RNG:  next_state = S_EXEC_1CYCLE;
              OP_LDI:  next_state = S_EXEC_LDI_LM;
              default: next_state = S_ERROR;
            endcase
          end

          FMT_DUAL: begin
            case (instr.dual.opcode)
              OP_MOV: next_state = S_EXEC_1CYCLE;
              // 2reg alu math instructions goto the 3cycle starting at alutmp latch 
              OP_ADD, OP_ADC, OP_SUB, OP_SBC, OP_AND, OP_XOR, OP_OR:
              next_state = S_EXEC_2R_MATH_TMP;

              OP_LDM:  next_state = S_EXEC_MAR_RB_L;
              OP_STM:  next_state = S_EXEC_MAR_RB_S;
              default: next_state = S_ERROR;
            endcase
          end

          default: next_state = S_ERROR;
        endcase
      end

      // EXECUTE STAGE:
      // --------------
      S_EXEC_1CYCLE: next_state = S_FETCH_IR;

      // 2reg alu math (3cycle)
      // S_EXEC_2R_MATH_TMP -> S_EXEC_2R_MATH_RES -> S_EXEC_2R_MATH_WB -> reset
      S_EXEC_2R_MATH_TMP: next_state = S_EXEC_2R_MATH_RES;
      S_EXEC_2R_MATH_RES: next_state = S_EXEC_2R_MATH_WB;
      S_EXEC_2R_MATH_WB:  next_state = S_FETCH_IR;

      // 1reg alu math (2cycle)
      // S_EXEC_1R_MATH_RES -> S_EXEC_1R_MATH_WB -> reset
      S_EXEC_1R_MATH_RES: next_state = S_EXEC_1R_MATH_WB;
      S_EXEC_1R_MATH_WB:  next_state = S_FETCH_IR;

      // load from memory 
      S_EXEC_MAR_RB_L: next_state = S_EXEC_RD_RAM;
      S_EXEC_RD_RAM:   next_state = S_FETCH_IR;
      // store to memory
      S_EXEC_MAR_RB_S: next_state = S_EXEC_WR_RAM;
      S_EXEC_WR_RAM:   next_state = S_FETCH_IR;

      // ldi
      S_EXEC_LDI_LM: next_state = S_EXEC_LDI_IP;
      S_EXEC_LDI_IP: next_state = S_FETCH_IR;

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
      // OP_JMP, OP_JMPR, OP_JC, OP_JNC
      S_EXEC_1CYCLE: begin
        case (instr_fmt)
          FMT_NONE: begin
            case (instr.none.opcode)
              OP_RST: ctrl.pc_rst = 1'b1;
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
              OP_JMP: begin
                ctrl.src_sel = BUS_SRC_RAM;
                ctrl.dst_sel = BUS_DST_PC;
              end
              OP_JC: begin
                if (carry_flag) begin
                  ctrl.src_sel = BUS_SRC_RAM;
                  ctrl.dst_sel = BUS_DST_PC;
                end else begin
                  ctrl.pc_inc = 1'b1;
                end
              end
              OP_JNC: begin
                if (!carry_flag) begin
                  ctrl.src_sel = BUS_SRC_RAM;
                  ctrl.dst_sel = BUS_DST_PC;
                end else begin
                  ctrl.pc_inc = 1'b1;
                end
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
              OP_JMPR: begin
                ctrl.src_sel = BUS_SRC_REG_B;
                ctrl.dst_sel = BUS_DST_PC;
              end
              default: ;
            endcase
          end

          default: ;  // empty; zeroed at top
        endcase
      end

      // 2reg alu math (3cycle)
      // OP_ADD, OP_ADC, OP_SUB, OP_SBC, OP_AND, OP_XOR, OP_OR
      S_EXEC_2R_MATH_TMP: begin
        // latch operand b to alu temporary
        ctrl.src_sel = BUS_SRC_REG_B;
        ctrl.dst_sel = BUS_DST_ALUTMP;
      end

      S_EXEC_2R_MATH_RES: begin
        case (instr_fmt)
          FMT_DUAL: begin
            // throw operand a onto the bus & latch alures
            ctrl.src_sel = BUS_SRC_REG_A;
            ctrl.dst_sel = BUS_DST_ALURES;

            // aluop selecting (& flag setting)
            case (instr.dual.opcode)
              OP_ADD: begin
                ctrl.aluop = ALU_ADD;
                ctrl.flgs_c_we = 1'b1;  // ADD updates carry (but does not consume)  
              end
              OP_ADC: begin
                ctrl.aluop = ALU_ADC;
                ctrl.flgs_c_we = 1'b1;
              end
              OP_SUB: begin
                ctrl.aluop = ALU_SUB;
                ctrl.flgs_c_we = 1'b1;  // SUB updates carry (but does not consume)  
              end
              OP_SBC: begin
                ctrl.aluop = ALU_SBC;
                ctrl.flgs_c_we = 1'b1;
              end
              OP_OR:   ctrl.aluop = ALU_OR;
              OP_XOR:  ctrl.aluop = ALU_XOR;
              OP_AND:  ctrl.aluop = ALU_AND;
              default: ;
            endcase
          end
          default: ;
        endcase
      end

      S_EXEC_2R_MATH_WB: begin
        ctrl.src_sel = BUS_SRC_ALURES;
        ctrl.dst_sel = BUS_DST_REG_A;
      end

      // 1reg alu math (2cycle)
      S_EXEC_1R_MATH_RES: begin
        case (instr_fmt)
          FMT_SINGLE: begin
            // throw single operand (b) onto bus and latch result
            ctrl.src_sel = BUS_SRC_REG_B;
            ctrl.dst_sel = BUS_DST_ALURES;

            // aluop and flags setting
            case (instr.single.opcode)
              OP_ROR: begin
                ctrl.aluop = ALU_ROR;
                ctrl.flgs_c_we = 1'b1;
              end
              OP_SHR: begin
                ctrl.aluop = ALU_SHR;
                ctrl.flgs_c_we = 1'b1;
              end
              OP_NOT:  ctrl.aluop = ALU_NOT;
              OP_INC:  ctrl.aluop = ALU_INC;
              OP_DEC:  ctrl.aluop = ALU_DEC;
              default: ;
            endcase
          end
          default: ;
        endcase
      end

      S_EXEC_1R_MATH_WB: begin
        // this is the same as S_EXEC_2R_MATH_WB
        // kept seperate for ease of reading the state flow
        ctrl.src_sel = BUS_SRC_ALURES;
        ctrl.dst_sel = BUS_DST_REG_B;
      end

      S_EXEC_MAR_RB_L: begin
        // same as S_EXEC_MAR_RB_S, duplicated for fsm flow
        ctrl.src_sel = BUS_SRC_REG_B;
        ctrl.dst_sel = BUS_DST_MAR;
      end

      S_EXEC_MAR_RB_S: begin
        // same as S_EXEC_MAR_RB_L, duplicated for fsm flow
        ctrl.src_sel = BUS_SRC_REG_B;
        ctrl.dst_sel = BUS_DST_MAR;
      end

      S_EXEC_RD_RAM: begin
        // select indexing to MAR address, load from ram.
        ctrl.addr_sel = 1'b1;
        ctrl.src_sel  = BUS_SRC_RAM;
        ctrl.dst_sel  = BUS_DST_REG_A;
      end

      S_EXEC_WR_RAM: begin
        // select indexing to MAR address, write to ram.
        ctrl.addr_sel = 1'b1;
        ctrl.src_sel  = BUS_SRC_REG_A;
        ctrl.dst_sel  = BUS_DST_RAM;
      end

      S_EXEC_LDI_LM: begin
        // pc is already pointing at the imm8 from the fetch cycle
        // just move it into rb.
        ctrl.src_sel = BUS_SRC_RAM;
        ctrl.dst_sel = BUS_DST_REG_B;
      end

      S_EXEC_LDI_IP: ctrl.pc_inc = 1'b1;

      default: ;  // empty; zeroed at top
    endcase
  end

endmodule
