// Copyright 2022 All Rights Reserved.
// Author: lanzongwei541@gmail.com (lanzongwei)
//
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the CPU0 target.
//
//===----------------------------------------------------------------------===//

#include "Cpu0ISelDAGToDAG.h"
#include "Cpu0.h"
#include "Cpu0MachineFunction.h"
#include "Cpu0RegisterInfo.h"
#include "Cpu0SEISelDAGToDAG.h"
#include "Cpu0TargetMachine.h"

#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
using namespace llvm;

#define DEBUG_TYPE "cpu0-isel"

//===----------------------------------------------------------------------===//
// Instruction Selector Implementation
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Cpu0DAGToDAGISel - CPU0 specific code to select CPU0 machine
// instructions for SelectionDAG operations.
//===----------------------------------------------------------------------===//

bool Cpu0DAGToDAGISel::runOnMachineFunction(MachineFunction &MF) {
  Subtarget = &MF.getSubtarget<Cpu0Subtarget>();
  bool Ret = SelectionDAGISel::runOnMachineFunction(MF);

  return Ret;
}

#if 0 // CH >= CH6_1 //1
/// getGlobalBaseReg - Output the instructions required to put the
/// GOT address into a register.
SDNode *Cpu0DAGToDAGISel::getGlobalBaseReg() {
  unsigned GlobalBaseReg = MF->getInfo<Cpu0FunctionInfo>()->getGlobalBaseReg();
  return CurDAG->getRegister(GlobalBaseReg, getTargetLowering()->getPointerTy(
                                                CurDAG->getDataLayout()))
      .getNode();
}
#endif

/// ComplexPattern used on Cpu0InstrInfo
/// Used on Cpu0 Load/Store instructions
bool Cpu0DAGToDAGISel::selectAddr(SDNode *Parent, SDValue Addr, SDValue &Base,
                                  SDValue &Offset) {
  EVT ValTy = Addr.getValueType();
  SDLoc DL(Addr);

  // If Parent is an unaligned f32 load or store, select a (base + index)
  // floating point load/store instruction (luxc1 or suxc1).
  const LSBaseSDNode *LS = 0;

  if (Parent && (LS = dyn_cast<LSBaseSDNode>(Parent))) {
    EVT VT = LS->getMemoryVT();

    if (VT.getSizeInBits() / 8 > LS->getAlignment()) {
      assert(0 && "Unaligned loads/stores not supported for this type.");
      if (VT == MVT::f32)
        return false;
    }
  }

  // if Address is FI, get the TargetFrameIndex.
  if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), ValTy);
    Offset = CurDAG->getTargetConstant(0, DL, ValTy);
    return true;
  }

#if 0 // CH >= CH6_1 //2
  // on PIC code Load GA
  if (Addr.getOpcode() == Cpu0ISD::Wrapper) {
    Base   = Addr.getOperand(0);
    Offset = Addr.getOperand(1);
    return true;
  }

  //@static
  if (TM.getRelocationModel() != Reloc::PIC_) {
    if ((Addr.getOpcode() == ISD::TargetExternalSymbol ||
        Addr.getOpcode() == ISD::TargetGlobalAddress))
      return false;
  }
#endif

#if 0 // CH >= CH7_1 //1
  // Addresses of the form FI+const or FI|const
  if (CurDAG->isBaseWithConstantOffset(Addr)) {
    ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(1));
    if (isInt<16>(CN->getSExtValue())) {

      // If the first operand is a FI, get the TargetFI Node
      if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>
                                  (Addr.getOperand(0)))
        Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), ValTy);
      else
        Base = Addr.getOperand(0);

      Offset = CurDAG->getTargetConstant(CN->getZExtValue(), DL, ValTy);
      return true;
    }
  }
#endif

  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, DL, ValTy);
  return true;
}

//@Select {
/// Select instructions not customized! Used for
/// expanded, promoted and normal instructions
void Cpu0DAGToDAGISel::Select(SDNode *Node) {
  //@Select }
  unsigned Opcode = Node->getOpcode();

  // If we have a custom node, we already have selected!
  if (Node->isMachineOpcode()) {
    LLVM_DEBUG(errs() << "== "; Node->dump(CurDAG); errs() << "\n");
    Node->setNodeId(-1);
    return;
  }

  // See if subclasses can handle this node.
  if (trySelect(Node))
    return;

  switch (Opcode) {
  default:
    break;

#if 0 // CH >= CH6_1 //3
  // Get target GOT address.
  case ISD::GLOBAL_OFFSET_TABLE:
    ReplaceNode(Node, getGlobalBaseReg());
    return;
#endif
  }

  // Select the default instruction
  SelectCode(Node);
}

#if 0 // CH >= CH11_2
// inlineasm begin
bool Cpu0DAGToDAGISel::
SelectInlineAsmMemoryOperand(const SDValue &Op, unsigned ConstraintID,
                             std::vector<SDValue> &OutOps) {
  // All memory constraints can at least accept raw pointers.
  switch(ConstraintID) {
  default:
    llvm_unreachable("Unexpected asm memory constraint");
  case InlineAsm::Constraint_m:
    OutOps.push_back(Op);
    return false;
  }
  return true;
}
// inlineasm end
#endif