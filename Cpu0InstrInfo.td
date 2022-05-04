//===-- Cpu0InstrInfo.td - Cpu0 Instruction defs -----------*- tablegen -*-===//
//
//                    The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the Cpu0 implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Cpu0 profiles and nodes
//===----------------------------------------------------------------------===//


def SDT_Cpu0Ret          : SDTypeProfile<0, 1, [SDTCisInt<0>]>;

def SDT_Cpu0DivRem       : SDTypeProfile<0, 2,
                                         [SDTCisInt<0>,
                                          SDTCisSameAs<0, 1>]>;

def SDT_Cpu0JmpLink      : SDTypeProfile<0, 1, [SDTCisVT<0, iPTR>]>;

def Cpu0JmpLink : SDNode<"Cpu0ISD::JmpLink",SDT_Cpu0JmpLink,
                         [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue,
                          SDNPVariadic]>;

// Hi and Lo nodes are used to handle global addresses. Used on
// Cpu0ISelLowering to lower stuff like GlobalAddress, ExternalSymbol
// static model. (nothing to do with Cpu0 Registers Hi and Lo)
def Cpu0Hi    : SDNode<"Cpu0ISD::Hi", SDTIntUnaryOp>;
def Cpu0Lo    : SDNode<"Cpu0ISD::Lo", SDTIntUnaryOp>;
def Cpu0GPRel : SDNode<"Cpu0ISD::GPRel", SDTIntUnaryOp>;

// Return
def Cpu0Ret : SDNode<"Cpu0ISD::Ret", SDTNone,
                     [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

// DivRem(u) nodes
def Cpu0DivRem    : SDNode<"Cpu0ISD::DivRem", SDT_Cpu0DivRem,
                           [SDNPOutGlue]>;
def Cpu0DivRemU   : SDNode<"Cpu0ISD::DivRemU", SDT_Cpu0DivRem,
                           [SDNPOutGlue]>;

def Cpu0Wrapper    : SDNode<"Cpu0ISD::Wrapper", SDTIntBinOp>;
//===----------------------------------------------------------------------===//
// Cpu0 Instruction Predicate Definitions.
//===----------------------------------------------------------------------===//
def EnableOverflow  : Predicate<"Subtarget->enableOverflow()">;
def DisableOverflow : Predicate<"Subtarget->disableOverflow()">;

def HasCmp          : Predicate<"Subtarget->hasCmp()">;
def HasSlt          : Predicate<"Subtarget->hasSlt()">;

def RelocPIC    :     Predicate<"TM.getRelocationModel() == Reloc::PIC_">;

class Cpu0InstAlias<string Asm, dag Result, bit Emit = 0b1> :
  InstAlias<Asm, Result, Emit>;
//===----------------------------------------------------------------------===//
// Instruction format superclass
//===----------------------------------------------------------------------===//

include "Cpu0InstrFormats.td"

//===----------------------------------------------------------------------===//
// Cpu0 Operand, Complex Patterns and Transformations Definitions
//===----------------------------------------------------------------------===//
// Instruction operand types

// BEQ, BNE
def brtarget16    : Operand<OtherVT> {
  let EncoderMethod = "getBranch16TargetOpValue";
  let OperandType = "OPERAND_PCREL";
  let DecoderMethod = "DecodeBranch16Target";
}

// JEQ, JNE, ...
def brtarget24    : Operand<OtherVT> {
  let EncoderMethod = "getBranch24TargetOpValue";
  let OperandType = "OPERAND_PCREL";
  let DecoderMethod = "DecodeBranch24Target";
}

// JMP
def jmptarget    : Operand<OtherVT> {
  let EncoderMethod = "getJumpTargetOpValue";
  let OperandType = "OPERAND_PCREL";
}

def calltarget  : Operand<iPTR> {
  let EncoderMethod = "getJumpTargetOpValue";
  let OperandType = "OPERAND_PCREL";
}

// Signed Operand
def simm16 : Operand<i32> {
  let DecoderMethod = "DecodeSimm16";
}

def shamt       : Operand<i32>;

// Unsigned Operand
def uimm16      : Operand<i32> {
  let PrintMethod = "printUnsignedImm";
}

def Cpu0MemAsmOperand : AsmOperandClass {
  let Name = "Mem";
  let ParserMethod = "parseMemOperand";
}

// Address operand
def mem : Operand<iPTR> {
  let PrintMethod = "printMemOperand";
  let MIOperandInfo = (ops GPROut, simm16);
  let EncoderMethod = "getMemEncoding";
  let ParserMatchClass = Cpu0MemAsmOperand;
}

def mem_ea : Operand<iPTR> {
  let PrintMethod = "printMemOperandEA";
  let MIOperandInfo = (ops GPROut, simm16);
  let EncoderMethod = "getMemEncoding";
}

// Transformation Function - get the lower 16 bits.
def LO16 : SDNodeXForm<imm, [{
  return getImm(N, N->getZExtValue() & 0xffff);
}]>;

// Transformation Function - get the higher 16 bits.
def HI16 : SDNodeXForm<imm, [{
  return getImm(N, (N->getZExtValue() >> 16) & 0xffff);
}]>;

// Node immediate fits as 16-bit sign extended on target immediate.
// e.g. addi, andi
def immSExt16 : PatLeaf<(imm), [{ return isInt<16>(N->getSExtValue()); }]>;

// Node immediate fits as 16-bit zero extended on target immediate.
// The LO16 param means that only the lower 16 bits of the node
// immediate are caught.
// e.g. addiu, sltiu
def immZExt16  : PatLeaf<(imm), [{
  if (N->getValueType(0) == MVT::i32)
    return (uint32_t)N->getZExtValue() == (unsigned short)N->getZExtValue();
  else
    return (uint64_t)N->getZExtValue() == (unsigned short)N->getZExtValue();
}], LO16>;

// Immediate can be loaded with LUi (32-bit int with lower 16-bit cleared).
def immLow16Zero : PatLeaf<(imm), [{
  int64_t Val = N->getSExtValue();
  return isInt<32>(Val) && !(Val & 0xffff);
}]>;

// shamt field must fit in 5 bits.
def immZExt5 : ImmLeaf<i32, [{return Imm == (Imm & 0x1f);}]>;

// Cpu0 Address Mode! SDNode frameindex could possibily be a match
// since load and store instructions from stack used it.
def addr : 
  ComplexPattern<iPTR, 2, "selectAddr", [frameindex], [SDNPWantParent]>;

//===----------------------------------------------------------------------===//
// Pattern fragment for load/store
//===----------------------------------------------------------------------===//

class AlignedLoad<PatFrag Node>
  : PatFrag<(ops node:$ptr), (Node node:$ptr),
            [{
              LoadSDNode *LD = cast<LoadSDNode>(N);
              return LD->getMemoryVT().getSizeInBits()/8 <= LD->getAlignment();
            }]>;

class AlignedStore<PatFrag Node>
  : PatFrag<(ops node:$val, node:$ptr), (Node node:$val, node:$ptr),
            [{
              StoreSDNode *ST = cast<StoreSDNode>(N);
              return ST->getMemoryVT().getSizeInBits()/8 <= ST->getAlignment();
            }]>;

// Load/Store PatFrags
def sextloadi16_a   : AlignedLoad<sextloadi16>;
def zextloadi16_a   : AlignedLoad<zextloadi16>;
def extloadi16_a    : AlignedLoad<extloadi16>;
def load_a          : AlignedLoad<load>;
def truncstorei16_a : AlignedStore<truncstorei16>;
def store_a         : AlignedStore<store>;

//===----------------------------------------------------------------------===//
// Instructions specific format
//===----------------------------------------------------------------------===//

class CmpInstr<bits<8> op, string instr_asm, 
               InstrItinClass itin, RegisterClass RC, RegisterClass RD, 
               bit isComm = 0>:
  FA<op, (outs RD:$ra), (ins RC:$rb, RC:$rc),
     !strconcat(instr_asm, "\t$ra, $rb, $rc"), [], itin> {
  let shamt = 0;
  let isCommutable = isComm;
  let Predicates = [HasCmp];
}

// Arithmetic and logical instructions with 3 register operands.
class ArithLogicR<bits<8> op, string instr_asm, SDNode OpNode,
                  InstrItinClass itin, RegisterClass RC, bit isComm = 0>:
  FA<op, (outs GPROut:$ra), (ins RC:$rb, RC:$rc),
     !strconcat(instr_asm, "\t$ra, $rb, $rc"),
     [(set GPROut:$ra, (OpNode RC:$rb, RC:$rc))], itin> {
  let shamt = 0;
  let isCommutable = isComm;	// e.g. add rb rc =  add rc rb
  let isReMaterializable = 1;
}

// Arithmetic and logical instructions with 2 register operands
class ArithLogicI<bits<8> op, string instrAsm, SDNode opNode,
                  Operand od, PatLeaf immType, RegisterClass regClass>
  : FL<op, (outs GPROut:$ra), (ins regClass:$rb, od:$imm16),
       !strconcat(instrAsm, "\t$ra, $rb, $imm16"),
       [(set GPROut:$ra, (opNode regClass:$rb, immType:$imm16))], IIAlu> {
  let isReMaterializable = 1;
}

//  Logical
class LogicNOR<bits<8> op, string instr_asm, RegisterClass RC>:
  FA<op, (outs RC:$ra), (ins RC:$rb, RC:$rc),
     !strconcat(instr_asm, "\t$ra, $rb, $rc"),
     [(set RC:$ra, (not (or RC:$rb, RC:$rc)))], IIAlu> {
  let shamt = 0;
  let isCommutable = 1;
}

// Shifts
class shift_rotate_imm<bits<8> op, string instr_asm,
                       SDNode OpNode, PatFrag PF, Operand ImmOpnd,
                       RegisterClass RC>:
  FA<op, (outs GPROut:$ra), (ins RC:$rb, ImmOpnd:$shamt),
     !strconcat(instr_asm, "\t$ra, $rb, $shamt"),
     [(set GPROut:$ra, (OpNode RC:$rb, PF:$shamt))], IIAlu> {
  let rc = 0;
}

// 32-bit shift instructions.
class shift_rotate_imm32<bits<8> op, string instr_asm,
                         SDNode OpNode>:
  shift_rotate_imm<op, instr_asm, OpNode, immZExt5, shamt, CPURegs>;

class shift_rotate_reg<bits<8> op, string instr_asm,
                       SDNode OpNode, RegisterClass RC>:
  FA<op, (outs GPROut:$ra), (ins RC:$rb, RC:$rc),
     !strconcat(instr_asm, "\t$ra, $rb, $rc"),
     [(set GPROut:$ra, (OpNode RC:$rb, RC:$rc))], IIAlu> {
  let shamt = 0;
}

// Load Upper Imediate
class LoadUpper<bits<8> op, string instr_asm, RegisterClass RC, Operand Imm>:
  FL<op, (outs RC:$ra), (ins Imm:$imm16),
     !strconcat(instr_asm, "\t$ra, $imm16"), [], IIAlu> {
  let rb = 0;
  let isReMaterializable = 1;
}

class FMem<bits<8> op, dag outs, dag ins, string asmStr, list<dag> pattern,
           InstrItinClass itin>
  : FL<op, outs, ins, asmStr, pattern, itin> {
  bits<20> addr;
  let Inst{19-16}   = addr{19-16};
  let Inst{15-0}    = addr{15-0};
  let DecoderMethod = "DecodeMem";
}

// Memory Load/Store
let canFoldAsLoad = 1 in
class LoadM<bits<8> op, string instrAsm, PatFrag opNode, RegisterClass regClass,
            Operand od, bit pseudo>
  : FMem<op, (outs regClass:$ra), (ins od:$addr),
         !strconcat(instrAsm, "\t$ra, $addr"),
         [(set regClass:$ra, (opNode addr:$addr))], IILoad> {
  let isPseudo = pseudo;
}

class StoreM<bits<8> op, string instrAsm, PatFrag opNode, RegisterClass regClass,
             Operand od, bit pseudo>
  : FMem<op, (outs), (ins regClass:$ra, od:$addr),
         !strconcat(instrAsm, "\t$ra, $addr"),
         [(opNode regClass:$ra, addr:$addr)], IIStore> {
  let isPseudo = pseudo;
}

// 32-bit load
class LoadM32<bits<8> op, string instrAsm,
                   PatFrag opNode, bit pseudo=0>
  : LoadM<op, instrAsm, opNode, GPROut, mem, pseudo>;

// 32-bit store
class StoreM32<bits<8> op, string instrAsm,
                    PatFrag opNode, bit pseudo=0>
  : StoreM<op, instrAsm, opNode, CPURegs, mem, pseudo>;

// Conditional Branch, e.g. JEQ brtarget24
class CBranch24<bits<8> op, string instr_asm, RegisterClass RC>:
  FJ<op, (outs), (ins RC:$ra, brtarget24:$addr),
             !strconcat(instr_asm, "\t$ra, $addr"),
             [], IIBranch>, Requires<[HasCmp]> {
  let isBranch = 1;
  let isTerminator = 1;
  let hasDelaySlot = 1;
//  let Predicates = [HasCmp]; // same effect as Requires
}

// Conditional Branch, e.g. BEQ $r1, $r2, brtarget16
class CBranch16<bits<8> op, string instr_asm, PatFrag cond_op, RegisterClass RC>:
  FL<op, (outs), (ins RC:$ra, RC:$rb, brtarget16:$imm16),
             !strconcat(instr_asm, "\t$ra, $rb, $imm16"),
             [(brcond (i32 (cond_op RC:$ra, RC:$rb)), bb:$imm16)], IIBranch>, 
             Requires<[HasSlt]> {
  let isBranch = 1;
  let isTerminator = 1;
  let hasDelaySlot = 1;
  let Defs = [AT];
}

class SetCC_R<bits<8> op, string instr_asm, PatFrag cond_op,
              RegisterClass RC>:
  FA<op, (outs GPROut:$ra), (ins RC:$rb, RC:$rc),
     !strconcat(instr_asm, "\t$ra, $rb, $rc"),
     [(set GPROut:$ra, (cond_op RC:$rb, RC:$rc))],
     IIAlu>, Requires<[HasSlt]> {
  let shamt = 0;
}

class SetCC_I<bits<8> op, string instr_asm, PatFrag cond_op, Operand Od,
              PatLeaf imm_type, RegisterClass RC>:
  FL<op, (outs GPROut:$ra), (ins RC:$rb, Od:$imm16),
     !strconcat(instr_asm, "\t$ra, $rb, $imm16"),
     [(set GPROut:$ra, (cond_op RC:$rb, imm_type:$imm16))],
     IIAlu>, Requires<[HasSlt]> {
}

class UncondBranch<bits<8> op, string instr_asm>:
  FJ<op, (outs), (ins jmptarget:$addr),
             !strconcat(instr_asm, "\t$addr"), [(br bb:$addr)], IIBranch> {
  let isBranch = 1;
  let isTerminator = 1;
  let isBarrier = 1;
  let hasDelaySlot = 0;
}

// JumpFR
let isBranch=1, isTerminator=1, isBarrier=1, hasDelaySlot=1,
    isIndirectBranch=1 in
class JumpFR<bits<8> op, string instrAsm, RegisterClass regClass>
  : FL<op, (outs), (ins regClass:$ra),
       !strconcat(instrAsm, "\t$ra"), [(brind regClass:$ra)], IIBranch> {
  let rb = 0;
  let imm16 = 0;
  let DecoderMethod = "DecodeJumpFR";
}

let isCall=1, hasDelaySlot=1 in {
  class JumpLink<bits<8> op, string instr_asm>:
    FJ<op, (outs), (ins calltarget:$target, variable_ops),
       !strconcat(instr_asm, "\t$target"), [(Cpu0JmpLink imm:$target)],
       IIBranch> {
       let DecoderMethod = "DecodeJumpTarget";
       }

  class JumpLinkReg<bits<8> op, string instr_asm,
                    RegisterClass RC>:
    FA<op, (outs), (ins RC:$rb, variable_ops),
       !strconcat(instr_asm, "\t$rb"), [(Cpu0JmpLink RC:$rb)], IIBranch> {
    let rc = 0;
    let ra = 14;
    let shamt = 0;
  }
}

// Mul, Div
class Mult<bits<8> op, string instr_asm, InstrItinClass itin,
           RegisterClass RC, list<Register> DefRegs>:
  FA<op, (outs), (ins RC:$ra, RC:$rb),
     !strconcat(instr_asm, "\t$ra, $rb"), [], itin> {
  let rc = 0;
  let shamt = 0;
  let isCommutable = 1;
  let Defs = DefRegs;
  let hasSideEffects = 0;
}

class Mult32<bits<8> op, string instr_asm, InstrItinClass itin>:
  Mult<op, instr_asm, itin, CPURegs, [HI, LO]>;

class Div<SDNode opNode, bits<8> op, string instr_asm, InstrItinClass itin,
          RegisterClass RC, list<Register> DefRegs>:
  FA<op, (outs), (ins RC:$ra, RC:$rb),
     !strconcat(instr_asm, "\t$ra, $rb"),
     [(opNode RC:$ra, RC:$rb)], itin> {
  let rc = 0;
  let shamt = 0;
  let Defs = DefRegs;
}

class Div32<SDNode opNode, bits<8> op, string instr_asm, InstrItinClass itin>:
  Div<opNode, op, instr_asm, itin, CPURegs, [HI, LO]>;

// Move from Lo/Hi
class MoveFromLOHI<bits<8> op, string instr_asm, RegisterClass RC,
                   list<Register> UseRegs>:
  FA<op, (outs RC:$ra), (ins),
     !strconcat(instr_asm, "\t$ra"), [], IIHiLo> {
  let rb = 0;
  let rc = 0;
  let shamt = 0;
  let Uses = UseRegs;
  let hasSideEffects = 0;
}

// Move to Lo/Hi
class MoveToLOHI<bits<8> op, string instr_asm, RegisterClass RC,
                 list<Register> DefRegs>:
  FA<op, (outs), (ins RC:$ra),
     !strconcat(instr_asm, "\t$ra"), [], IIHiLo> {
  let rb = 0;
  let rc = 0;
  let shamt = 0;
  let Defs = DefRegs;
  let hasSideEffects = 0;
}

// Move from C0 (co-processor 0) Register
class MoveFromC0<bits<8> op, string instr_asm, RegisterClass RC>:
  FA<op, (outs), (ins RC:$ra, C0Regs:$rb),
     !strconcat(instr_asm, "\t$ra, $rb"), [], IIAlu> {
  let rc = 0;
  let shamt = 0;
  let hasSideEffects = 0;
}

// Move to C0 Register
class MoveToC0<bits<8> op, string instr_asm, RegisterClass RC>:
  FA<op, (outs C0Regs:$ra), (ins RC:$rb),
     !strconcat(instr_asm, "\t$ra, $rb"), [], IIAlu> {
  let rc = 0;
  let shamt = 0;
  let hasSideEffects = 0;
}

// Move from C0 register to C0 register
class C0Move<bits<8> op, string instr_asm>:
  FA<op, (outs C0Regs:$ra), (ins C0Regs:$rb),
     !strconcat(instr_asm, "\t$ra, $rb"), [], IIAlu> {
  let rc = 0;
  let shamt = 0;
  let hasSideEffects = 0;
}

// Count Leading Ones/Zeros in Word
class CountLeading0<bits<8> op, string instr_asm, RegisterClass RC>:
  FA<op, (outs GPROut:$ra), (ins RC:$rb),
     !strconcat(instr_asm, "\t$ra, $rb"),
     [(set GPROut:$ra, (ctlz RC:$rb))], IICLZ> {
  let rc = 0;
  let shamt = 0;
}

class CountLeading1<bits<8> op, string instr_asm, RegisterClass RC>:
  FA<op, (outs GPROut:$ra), (ins RC:$rb),
     !strconcat(instr_asm, "\t$ra, $rb"),
     [(set GPROut:$ra, (ctlz (not RC:$rb)))], IICLO> {
  let rc = 0;
  let shamt = 0;
}

//@EffectiveAddress
class EffectiveAddress<string instr_asm, RegisterClass RC, Operand Mem> :
  FMem<0x09, (outs RC:$ra), (ins Mem:$addr),
     instr_asm, [(set RC:$ra, addr:$addr)], IIAlu>;

// Return
class RetBase<RegisterClass regClass> : JumpFR<0x3c, "ret", regClass> {
  let isReturn = 1;
  let isCodeGenOnly = 1;
  let hasCtrlDep = 1;
  let hasExtraSrcRegAllocReq = 1;
}

// We need these two pseudo instructions to avoid offset calculation for long
// branches.  See the comment in file Cpu0LongBranch.cpp for detailed
// explanation.

// Expands to: lui $dst, %hi($tgt - $baltgt)
def LONG_BRANCH_LUi : Cpu0Pseudo<(outs GPROut:$dst),
  (ins jmptarget:$tgt, jmptarget:$baltgt), "", []>;

// Expands to: addiu $dst, $src, %lo($tgt - $baltgt)
def LONG_BRANCH_ADDiu : Cpu0Pseudo<(outs GPROut:$dst),
  (ins GPROut:$src, jmptarget:$tgt, jmptarget:$baltgt), "", []>;

//===----------------------------------------------------------------------===//
// Instruction definition
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Pseudo Instruction definition
//===----------------------------------------------------------------------===//

class LoadImm32< string instr_asm, Operand Od, RegisterClass RC> :
  Cpu0AsmPseudoInst<(outs RC:$ra), (ins Od:$imm32),
                     !strconcat(instr_asm, "\t$ra, $imm32")> ;
def LoadImm32Reg : LoadImm32<"li", shamt, GPROut>;

class LoadAddress<string instr_asm, Operand MemOpnd, RegisterClass RC> :
  Cpu0AsmPseudoInst<(outs RC:$ra), (ins MemOpnd:$addr),
                     !strconcat(instr_asm, "\t$ra, $addr")> ;
def LoadAddr32Reg : LoadAddress<"la", mem, GPROut>;

class LoadAddressImm<string instr_asm, Operand Od, RegisterClass RC> :
  Cpu0AsmPseudoInst<(outs RC:$ra), (ins Od:$imm32),
                     !strconcat(instr_asm, "\t$ra, $imm32")> ;
def LoadAddr32Imm : LoadAddressImm<"la", shamt, GPROut>;

// Load and Store Instructions
// already aligned
def LD      : LoadM32<0x01,   "ld",   load_a>;
def ST      : StoreM32<0x02,  "st",   store_a>;
def LB     : LoadM32<0x03, "lb",  sextloadi8>;
def LBu    : LoadM32<0x04, "lbu", zextloadi8>;
def SB     : StoreM32<0x05, "sb", truncstorei8>;
def LH     : LoadM32<0x06, "lh",  sextloadi16_a>;
def LHu    : LoadM32<0x07, "lhu", zextloadi16_a>;
def SH     : StoreM32<0x08, "sh", truncstorei16_a>;

// Arithmetic Instructions (ALU Immediate)
def ADDiu    : ArithLogicI<0x09, "addiu", add, simm16, immSExt16, CPURegs>;
def ANDi    : ArithLogicI<0x0c, "andi", and, uimm16, immZExt16, CPURegs>;

def ORi     : ArithLogicI<0x0d, "ori", or, uimm16, immZExt16, CPURegs>;

def XORi    : ArithLogicI<0x0e, "xori", xor, uimm16, immZExt16, CPURegs>;
def LUi     : LoadUpper<0x0F, "lui", GPROut, uimm16>;
// Arithmetic Instructions (3-Operand, R-Type)

let Predicates = [HasCmp] in {
def CMP     : CmpInstr<0x2A, "cmp", IIAlu, CPURegs, SR, 0>;
def CMPu    : CmpInstr<0x2B, "cmpu", IIAlu, CPURegs, SR, 0>;
}

let Predicates = [DisableOverflow] in {
def ADDu    : ArithLogicR<0x11, "addu", add, IIAlu, CPURegs, 1>;
}

let Predicates = [DisableOverflow] in {
def SUBu    : ArithLogicR<0x12, "subu", sub, IIAlu, CPURegs>;
}
let Predicates = [EnableOverflow] in {
def ADD     : ArithLogicR<0x13, "add", add, IIAlu, CPURegs, 1>;
def SUB     : ArithLogicR<0x14, "sub", sub, IIAlu, CPURegs>;
}
def MUL     : ArithLogicR<0x17, "mul", mul, IIImul, CPURegs, 1>;

def AND     : ArithLogicR<0x18, "and", and, IIAlu, CPURegs, 1>;
def OR      : ArithLogicR<0x19, "or", or, IIAlu, CPURegs, 1>;
def XOR     : ArithLogicR<0x1a, "xor", xor, IIAlu, CPURegs, 1>;
def NOR     : LogicNOR<0x1b, "nor", CPURegs>;

// Shift Instructions

// sra is IR node for ashr llvm IR instruction of .bc
def ROL     : shift_rotate_imm32<0x1c, "rol", rotl>;
def ROR     : shift_rotate_imm32<0x1d, "ror", rotr>;

def SHL     : shift_rotate_imm32<0x1e, "shl", shl>;

// srl is IR node for lshr llvm IR instruction of .bc
def SHR     : shift_rotate_imm32<0x1f, "shr", srl>;
def SRA     : shift_rotate_imm32<0x20, "sra", sra>;
def SRAV    : shift_rotate_reg<0x21, "srav", sra, CPURegs>;
def SHLV    : shift_rotate_reg<0x22, "shlv", shl, CPURegs>;
def SHRV    : shift_rotate_reg<0x23, "shrv", srl, CPURegs>;
def ROLV    : shift_rotate_reg<0x24, "rolv", rotl, CPURegs>;
def RORV    : shift_rotate_reg<0x25, "rorv", rotr, CPURegs>;

let Predicates = [HasSlt] in {
def SLTi    : SetCC_I<0x26, "slti", setlt, simm16, immSExt16, CPURegs>;
def SLTiu   : SetCC_I<0x27, "sltiu", setult, simm16, immSExt16, CPURegs>;
def SLT     : SetCC_R<0x28, "slt", setlt, CPURegs>;
def SLTu    : SetCC_R<0x29, "sltu", setult, CPURegs>;
}

// Jump Instructions

let Predicates = [HasCmp] in {
def JEQ     : CBranch24<0x30, "jeq", SR>;
def JNE     : CBranch24<0x31, "jne", SR>;
def JLT     : CBranch24<0x32, "jlt", SR>;
def JGT     : CBranch24<0x33, "jgt", SR>;
def JLE     : CBranch24<0x34, "jle", SR>;
def JGE     : CBranch24<0x35, "jge", SR>;
}

let Predicates = [HasSlt] in {
def BEQ     : CBranch16<0x37, "beq", seteq, GPROut>;
def BNE     : CBranch16<0x38, "bne", setne, GPROut>;
}

def JMP     : UncondBranch<0x36, "jmp">;

//@ long branch support //1
let isBranch = 1, isTerminator = 1, isBarrier = 1,
    hasDelaySlot = 0, Defs = [LR] in
def BAL: FJ<0x3A, (outs), (ins jmptarget:$addr), "bal\t$addr", [], IIBranch>, 
             Requires<[HasSlt]>;

def JR       : JumpFR<0x3c, "jr", GPROut>;

let isReturn=1, isTerminator=1, hasDelaySlot=1, isBarrier=1, hasCtrlDep=1 in
  def RetLR : Cpu0Pseudo<(outs), (ins), "", [(Cpu0Ret)]>;

def RET      : RetBase<GPROut>;

/// Multiply and Divide Instructions.
def MULT    : Mult32<0x41, "mult", IIImul>;
def MULTu   : Mult32<0x42, "multu", IIImul>;
def SDIV    : Div32<Cpu0DivRem, 0x43, "div", IIIdiv>;
def UDIV    : Div32<Cpu0DivRemU, 0x44, "divu", IIIdiv>;

def MFHI    : MoveFromLOHI<0x46, "mfhi", CPURegs, [HI]>;
def MFLO    : MoveFromLOHI<0x47, "mflo", CPURegs, [LO]>;
def MTHI    : MoveToLOHI<0x48, "mthi", CPURegs, [HI]>;
def MTLO    : MoveToLOHI<0x49, "mtlo", CPURegs, [LO]>;

def MFC0    : MoveFromC0<0x50, "mfc0", CPURegs>;
def MTC0    : MoveToC0<0x51, "mtc0", CPURegs>;

def C0MOVE  : C0Move<0x52, "c0mov">;


// No Operation Instructions
let addr=0 in
  def NOP    : FJ<0, (outs), (ins), "nop", [], IIAlu>;

/// Count Leading
def CLZ : CountLeading0<0x15, "clz", CPURegs>;
def CLO : CountLeading1<0x16, "clo", CPURegs>;

//@def LEA_ADDiu {
// FrameIndexes are legalized when they are operands from load/store
// instructions. The same not happens for stack address copies, so an
// add op with mem ComplexPattern is used and the stack address copy
// can be matched. It's similar to Sparc LEA_ADDRi
def LEA_ADDiu : EffectiveAddress<"addiu\t$ra, $addr", CPURegs, mem_ea> {
  let isCodeGenOnly = 1;
}

//===----------------------------------------------------------------------===//
// Instruction aliases
//===----------------------------------------------------------------------===//
def : Cpu0InstAlias<"move $dst, $src",
                    (ADDu GPROut:$dst, GPROut:$src,ZERO), 1>;

//===----------------------------------------------------------------------===//
// Arbitrary patterns that map to one or more instructions
//===----------------------------------------------------------------------===//

// Small Immediates
def : Pat<(i32 immSExt16:$in), (ADDiu ZERO, imm:$in)>;

def : Pat<(i32 immZExt16:$in),
          (ORi ZERO, imm:$in)>;
def : Pat<(i32 immLow16Zero:$in),
          (LUi (HI16 imm:$in))>;

// Arbitrary immediates
def : Pat<(i32 imm:$imm),
          (ORi (LUi (HI16 imm:$imm)), (LO16 imm:$imm))>;

// Carry patterns
def : Pat<(subc CPURegs:$lhs, CPURegs:$rhs),
          (SUBu CPURegs:$lhs, CPURegs:$rhs)>;
def : Pat<(addc CPURegs:$lhs, CPURegs:$rhs),
          (ADDu CPURegs:$lhs, CPURegs:$rhs)>;
def : Pat<(addc  CPURegs:$src, immSExt16:$imm),
          (ADDiu CPURegs:$src, imm:$imm)>;

def : Pat<(not CPURegs:$in),
// 1's complement of in, ex. not(0xf000000f) == 0x0ffffff0
          (NOR CPURegs:$in, ZERO)>;

def : Pat<(i32 (extloadi1  addr:$src)), (LBu addr:$src)>;
def : Pat<(i32 (extloadi8  addr:$src)), (LBu addr:$src)>;
def : Pat<(i32 (extloadi16_a addr:$src)), (LHu addr:$src)>;

// hi/lo relocs
def : Pat<(Cpu0Hi tglobaladdr:$in), (LUi tglobaladdr:$in)>;

def : Pat<(Cpu0Hi tblockaddress:$in), (LUi tblockaddress:$in)>;
def : Pat<(Cpu0Hi tjumptable:$in), (LUi tjumptable:$in)>;

def : Pat<(Cpu0Lo tglobaladdr:$in), (ORi ZERO, tglobaladdr:$in)>;

def : Pat<(Cpu0Lo tblockaddress:$in), (ORi ZERO, tblockaddress:$in)>;
def : Pat<(Cpu0Lo tjumptable:$in), (ORi ZERO, tjumptable:$in)>;

def : Pat<(add CPURegs:$hi, (Cpu0Lo tglobaladdr:$lo)),
          (ORi CPURegs:$hi, tglobaladdr:$lo)>;

def : Pat<(add CPURegs:$hi, (Cpu0Lo tblockaddress:$lo)),
              (ORi CPURegs:$hi, tblockaddress:$lo)>;
def : Pat<(add CPURegs:$hi, (Cpu0Lo tjumptable:$lo)),
              (ORi CPURegs:$hi, tjumptable:$lo)>;

// gp_rel relocs
def : Pat<(add CPURegs:$gp, (Cpu0GPRel tglobaladdr:$in)),
          (ORi CPURegs:$gp, tglobaladdr:$in)>;

//@ wrapper_pic
class WrapperPat<SDNode node, Instruction ORiOp, RegisterClass RC>:
      Pat<(Cpu0Wrapper RC:$gp, node:$in),
              (ORiOp RC:$gp, node:$in)>;

def : WrapperPat<tglobaladdr, ORi, GPROut>;

def : WrapperPat<tjumptable, ORi, GPROut>;

// brcond for cmp instruction
multiclass BrcondPatsCmp<RegisterClass RC, Instruction JEQOp, Instruction JNEOp, 
Instruction JLTOp, Instruction JGTOp, Instruction JLEOp, Instruction JGEOp, 
Instruction CMPOp, Instruction CMPuOp, Register ZEROReg> {
def : Pat<(brcond (i32 (seteq RC:$lhs, RC:$rhs)), bb:$dst),
          (JEQOp (CMPOp RC:$lhs, RC:$rhs), bb:$dst)>;
def : Pat<(brcond (i32 (setueq RC:$lhs, RC:$rhs)), bb:$dst),
          (JEQOp (CMPuOp RC:$lhs, RC:$rhs), bb:$dst)>;
def : Pat<(brcond (i32 (setne RC:$lhs, RC:$rhs)), bb:$dst),
          (JNEOp (CMPOp RC:$lhs, RC:$rhs), bb:$dst)>;
def : Pat<(brcond (i32 (setune RC:$lhs, RC:$rhs)), bb:$dst),
          (JNEOp (CMPuOp RC:$lhs, RC:$rhs), bb:$dst)>;
def : Pat<(brcond (i32 (setlt RC:$lhs, RC:$rhs)), bb:$dst),
          (JLTOp (CMPOp RC:$lhs, RC:$rhs), bb:$dst)>;
def : Pat<(brcond (i32 (setult RC:$lhs, RC:$rhs)), bb:$dst),
          (JLTOp (CMPuOp RC:$lhs, RC:$rhs), bb:$dst)>;
def : Pat<(brcond (i32 (setgt RC:$lhs, RC:$rhs)), bb:$dst),
          (JGTOp (CMPOp RC:$lhs, RC:$rhs), bb:$dst)>;
def : Pat<(brcond (i32 (setugt RC:$lhs, RC:$rhs)), bb:$dst),
          (JGTOp (CMPuOp RC:$lhs, RC:$rhs), bb:$dst)>;
def : Pat<(brcond (i32 (setle RC:$lhs, RC:$rhs)), bb:$dst),
          (JLEOp (CMPOp RC:$lhs, RC:$rhs), bb:$dst)>;
def : Pat<(brcond (i32 (setule RC:$lhs, RC:$rhs)), bb:$dst),
          (JLEOp (CMPuOp RC:$lhs, RC:$rhs), bb:$dst)>;
def : Pat<(brcond (i32 (setge RC:$lhs, RC:$rhs)), bb:$dst),
          (JGEOp (CMPOp RC:$lhs, RC:$rhs), bb:$dst)>;
def : Pat<(brcond (i32 (setuge RC:$lhs, RC:$rhs)), bb:$dst),
          (JGEOp (CMPuOp RC:$lhs, RC:$rhs), bb:$dst)>;

def : Pat<(brcond RC:$cond, bb:$dst),
          (JNEOp (CMPOp RC:$cond, ZEROReg), bb:$dst)>;
}

// brcond for slt instruction
multiclass BrcondPatsSlt<RegisterClass RC, Instruction BEQOp, Instruction BNEOp,
                      Instruction SLTOp, Instruction SLTuOp, Register ZEROReg> {
def : Pat<(brcond (i32 (setne RC:$lhs, 0)), bb:$dst),
              (BNEOp RC:$lhs, ZEROReg, bb:$dst)>;
def : Pat<(brcond (i32 (seteq RC:$lhs, 0)), bb:$dst),
              (BEQOp RC:$lhs, ZEROReg, bb:$dst)>;

def : Pat<(brcond (i32 (seteq RC:$lhs, RC:$rhs)), bb:$dst),
              (BEQOp RC:$lhs, RC:$rhs, bb:$dst)>;
def : Pat<(brcond (i32 (setueq RC:$lhs, RC:$rhs)), bb:$dst),
              (BEQOp RC:$lhs, RC:$rhs, bb:$dst)>;
def : Pat<(brcond (i32 (setne RC:$lhs, RC:$rhs)), bb:$dst),
              (BNEOp RC:$lhs, RC:$rhs, bb:$dst)>;
def : Pat<(brcond (i32 (setune RC:$lhs, RC:$rhs)), bb:$dst),
              (BNEOp RC:$lhs, RC:$rhs, bb:$dst)>;
def : Pat<(brcond (i32 (setlt RC:$lhs, RC:$rhs)), bb:$dst),
              (BNE (SLTOp RC:$lhs, RC:$rhs), ZERO, bb:$dst)>;
def : Pat<(brcond (i32 (setult RC:$lhs, RC:$rhs)), bb:$dst),
              (BNE (SLTuOp RC:$lhs, RC:$rhs), ZERO, bb:$dst)>;
def : Pat<(brcond (i32 (setgt RC:$lhs, RC:$rhs)), bb:$dst),
              (BNE (SLTOp RC:$rhs, RC:$lhs), ZERO, bb:$dst)>;
def : Pat<(brcond (i32 (setugt RC:$lhs, RC:$rhs)), bb:$dst),
              (BNE (SLTuOp RC:$rhs, RC:$lhs), ZERO, bb:$dst)>;
def : Pat<(brcond (i32 (setle RC:$lhs, RC:$rhs)), bb:$dst),
              (BEQ (SLTOp RC:$rhs, RC:$lhs), ZERO, bb:$dst)>;
def : Pat<(brcond (i32 (setule RC:$lhs, RC:$rhs)), bb:$dst),
              (BEQ (SLTuOp RC:$rhs, RC:$lhs), ZERO, bb:$dst)>;
def : Pat<(brcond (i32 (setge RC:$lhs, RC:$rhs)), bb:$dst),
              (BEQ (SLTOp RC:$lhs, RC:$rhs), ZERO, bb:$dst)>;
def : Pat<(brcond (i32 (setuge RC:$lhs, RC:$rhs)), bb:$dst),
              (BEQ (SLTuOp RC:$lhs, RC:$rhs), ZERO, bb:$dst)>;

def : Pat<(brcond RC:$cond, bb:$dst),
              (BNEOp RC:$cond, ZEROReg, bb:$dst)>;
}

let Predicates = [HasSlt] in {
defm : BrcondPatsSlt<CPURegs, BEQ, BNE, SLT, SLTu, ZERO>;
}

let Predicates = [HasCmp] in {
defm : BrcondPatsCmp<CPURegs, JEQ, JNE, JLT, JGT, JLE, JGE, CMP, CMPu, ZERO>;
}

// setcc for cmp instruction
multiclass SeteqPatsCmp<RegisterClass RC> {
// a == b
  def : Pat<(seteq RC:$lhs, RC:$rhs),
            (SHR (ANDi (CMP RC:$lhs, RC:$rhs), 2), 1)>;
// a != b
  def : Pat<(setne RC:$lhs, RC:$rhs),
            (XORi (SHR (ANDi (CMP RC:$lhs, RC:$rhs), 2), 1), 1)>;
}

// a < b
multiclass SetltPatsCmp<RegisterClass RC> {
  def : Pat<(setlt RC:$lhs, RC:$rhs),
            (ANDi (CMP RC:$lhs, RC:$rhs), 1)>;
// if cpu0  `define N    `SW[31]  instead of `SW[0] // Negative flag, then need
// 2 more instructions as follows,
//          (XORi (ANDi (SHR (CMP RC:$lhs, RC:$rhs), (LUi 0x8000), 31), 1), 1)>;
  def : Pat<(setult RC:$lhs, RC:$rhs),
            (ANDi (CMPu RC:$lhs, RC:$rhs), 1)>;
}

// a <= b
multiclass SetlePatsCmp<RegisterClass RC> {
  def : Pat<(setle RC:$lhs, RC:$rhs),
// a <= b is equal to (XORi (b < a), 1)
            (XORi (ANDi (CMP RC:$rhs, RC:$lhs), 1), 1)>;
  def : Pat<(setule RC:$lhs, RC:$rhs),
            (XORi (ANDi (CMP RC:$rhs, RC:$lhs), 1), 1)>;
}

// a > b
multiclass SetgtPatsCmp<RegisterClass RC> {
  def : Pat<(setgt RC:$lhs, RC:$rhs),
// a > b is equal to b < a is equal to setlt(b, a)
            (ANDi (CMP RC:$rhs, RC:$lhs), 1)>;
  def : Pat<(setugt RC:$lhs, RC:$rhs),
            (ANDi (CMPu RC:$rhs, RC:$lhs), 1)>;
}

// a >= b
multiclass SetgePatsCmp<RegisterClass RC> {
  def : Pat<(setge RC:$lhs, RC:$rhs),
// a >= b is equal to b <= a
            (XORi (ANDi (CMP RC:$lhs, RC:$rhs), 1), 1)>;
  def : Pat<(setuge RC:$lhs, RC:$rhs),
            (XORi (ANDi (CMPu RC:$lhs, RC:$rhs), 1), 1)>;
}

// setcc for slt instruction
multiclass SeteqPatsSlt<RegisterClass RC, Instruction SLTiuOp, Instruction XOROp,
                     Instruction SLTuOp, Register ZEROReg> {
// a == b
  def : Pat<(seteq RC:$lhs, RC:$rhs),
                (SLTiuOp (XOROp RC:$lhs, RC:$rhs), 1)>;
// a != b
  def : Pat<(setne RC:$lhs, RC:$rhs),
                (SLTuOp ZEROReg, (XOROp RC:$lhs, RC:$rhs))>;
}

// a <= b
multiclass SetlePatsSlt<RegisterClass RC, Instruction SLTOp, Instruction SLTuOp> {
  def : Pat<(setle RC:$lhs, RC:$rhs),
// a <= b is equal to (XORi (b < a), 1)
                (XORi (SLTOp RC:$rhs, RC:$lhs), 1)>;
  def : Pat<(setule RC:$lhs, RC:$rhs),
                (XORi (SLTuOp RC:$rhs, RC:$lhs), 1)>;
}

// a > b
multiclass SetgtPatsSlt<RegisterClass RC, Instruction SLTOp, Instruction SLTuOp> {
  def : Pat<(setgt RC:$lhs, RC:$rhs),
// a > b is equal to b < a is equal to setlt(b, a)
                (SLTOp RC:$rhs, RC:$lhs)>;
  def : Pat<(setugt RC:$lhs, RC:$rhs),
                (SLTuOp RC:$rhs, RC:$lhs)>;
}

// a >= b
multiclass SetgePatsSlt<RegisterClass RC, Instruction SLTOp, Instruction SLTuOp> {
  def : Pat<(setge RC:$lhs, RC:$rhs),
// a >= b is equal to b <= a
                (XORi (SLTOp RC:$lhs, RC:$rhs), 1)>;
  def : Pat<(setuge RC:$lhs, RC:$rhs),
                (XORi (SLTuOp RC:$lhs, RC:$rhs), 1)>;
}

multiclass SetgeImmPatsSlt<RegisterClass RC, Instruction SLTiOp,
                        Instruction SLTiuOp> {
  def : Pat<(setge RC:$lhs, immSExt16:$rhs),
                (XORi (SLTiOp RC:$lhs, immSExt16:$rhs), 1)>;
  def : Pat<(setuge RC:$lhs, immSExt16:$rhs),
                (XORi (SLTiuOp RC:$lhs, immSExt16:$rhs), 1)>;
}

let Predicates = [HasSlt] in {
defm : SeteqPatsSlt<CPURegs, SLTiu, XOR, SLTu, ZERO>;
defm : SetlePatsSlt<CPURegs, SLT, SLTu>;
defm : SetgtPatsSlt<CPURegs, SLT, SLTu>;
defm : SetgePatsSlt<CPURegs, SLT, SLTu>;
defm : SetgeImmPatsSlt<CPURegs, SLTi, SLTiu>;
}

let Predicates = [HasCmp] in {
defm : SeteqPatsCmp<CPURegs>;
defm : SetltPatsCmp<CPURegs>;
defm : SetlePatsCmp<CPURegs>;
defm : SetgtPatsCmp<CPURegs>;
defm : SetgePatsCmp<CPURegs>;
}

