//===----------------------- Cpu0BranchExpansion.cpp ----------------------===//
// Copy from MipsBranchExpansion.cpp
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
///
/// This pass do two things:
/// - it expands a branch or jump instruction into a long branch if its offset
///   is too large to fit into its immediate field,
/// - it inserts nops to prevent forbidden slot hazards.
///
/// The reason why this pass combines these two tasks is that one of these two
/// tasks can break the result of the previous one.
///
/// Example of that is a situation where at first, no branch should be expanded,
/// but after adding at least one nop somewhere in the code to prevent a
/// forbidden slot hazard, offset of some branches may go out of range. In that
/// case it is necessary to check again if there is some branch that needs
/// expansion. On the other hand, expanding some branch may cause a control
/// transfer instruction to appear in the forbidden slot, which is a hazard that
/// should be fixed. This pass alternates between this two tasks untill no
/// changes are made. Only then we can be sure that all branches are expanded
/// properly, and no hazard situations exist.
///
/// Regarding branch expanding:
///
/// When branch instruction like beqzc or bnezc has offset that is too large
/// to fit into its immediate field, it has to be expanded to another
/// instruction or series of instructions.
///
/// FIXME: Fix pc-region jump instructions which cross 256MB segment boundaries.
/// TODO: Handle out of range bc, b (pseudo) instructions.
///
/// Regarding compact branch hazard prevention:
///
/// Hazards handled: forbidden slots for MIPSR6, FPU slots for MIPS3 and below,
/// load delay slots for MIPS1.
///
/// A forbidden slot hazard occurs when a compact branch instruction is executed
/// and the adjacent instruction in memory is a control transfer instruction
/// such as a branch or jump, ERET, ERETNC, DERET, WAIT and PAUSE.
///
/// For example:
///
/// 0x8004      bnec    a1,v0,<P+0x18>
/// 0x8008      beqc    a1,a2,<P+0x54>
///
/// In such cases, the processor is required to signal a Reserved Instruction
/// exception.
///
/// Here, if the instruction at 0x8004 is executed, the processor will raise an
/// exception as there is a control transfer instruction at 0x8008.
///
/// There are two sources of forbidden slot hazards:
///
/// A) A previous pass has created a compact branch directly.
/// B) Transforming a delay slot branch into compact branch. This case can be
///    difficult to process as lookahead for hazards is insufficient, as
///    backwards delay slot fillling can also produce hazards in previously
///    processed instuctions.
///
/// In future this pass can be extended (or new pass can be created) to handle
/// other pipeline hazards, such as various MIPS1 hazards, processor errata that
/// require instruction reorganization, etc.
///
/// This pass has to run after the delay slot filler as that pass can introduce
/// pipeline hazards such as compact branch hazard, hence the existing hazard
/// recognizer is not suitable.
///
//===----------------------------------------------------------------------===//

#include "Cpu0.h"
#include "Cpu0InstrInfo.h"
#include "Cpu0MachineFunction.h"
#include "Cpu0Subtarget.h"
#include "Cpu0TargetMachine.h"
#include "MCTargetDesc/Cpu0ABIInfo.h"
#include "MCTargetDesc/Cpu0BaseInfo.h"
#include "MCTargetDesc/Cpu0MCTargetDesc.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Target/TargetMachine.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <utility>

using namespace llvm;

#define DEBUG_TYPE "cpu0-branch-expansion"

STATISTIC(LongBranches, "Number of long branches.");

static cl::opt<bool>
    SkipLongBranch("skip-cpu0-long-branch", cl::init(false),
                   cl::desc("MIPS: Skip branch expansion pass."), cl::Hidden);

static cl::opt<bool>
    ForceLongBranch("force-cpu0-long-branch", cl::init(false),
                    cl::desc("MIPS: Expand all branches to long format."),
                    cl::Hidden);

namespace {

using Iter = MachineBasicBlock::iterator;
using ReverseIter = MachineBasicBlock::reverse_iterator;

struct MBBInfo {
  uint64_t Size = 0;
  bool HasLongBranch = false;
  MachineInstr *Br = nullptr;
  uint64_t Offset = 0;
  MBBInfo() = default;
};

class Cpu0BranchExpansion : public MachineFunctionPass {
public:
  static char ID;

  Cpu0BranchExpansion() : MachineFunctionPass(ID), ABI(Cpu0ABIInfo::Unknown()) {
    initializeCpu0BranchExpansionPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return "Cpu0 Branch Expansion Pass";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::NoVRegs);
  }

private:
  void splitMBB(MachineBasicBlock *MBB);
  void initMBBInfo();
  int64_t computeOffset(const MachineInstr *Br);
  uint64_t computeOffsetFromTheBeginning(int MBB);
  void replaceBranch(MachineBasicBlock &MBB, Iter Br, const DebugLoc &DL,
                     MachineBasicBlock *MBBOpnd);
  void expandToLongBranch(MBBInfo &Info);

  const Cpu0Subtarget *STI;
  const Cpu0InstrInfo *TII;

  MachineFunction *MFp;
  SmallVector<MBBInfo, 16> MBBInfos;
  bool IsPIC;
  Cpu0ABIInfo ABI;
  unsigned LongBranchSeqSize;
};

} // end of anonymous namespace

char Cpu0BranchExpansion::ID = 0;

INITIALIZE_PASS(Cpu0BranchExpansion, DEBUG_TYPE,
                "Expand out of range branch instructions and fix forbidden"
                " slot hazards",
                false, false)

/// Returns a pass that clears pipeline hazards.
FunctionPass *llvm::createCpu0BranchExpansion() {
  return new Cpu0BranchExpansion();
}

/// Iterate over list of Br's operands and search for a MachineBasicBlock
/// operand.
static MachineBasicBlock *getTargetMBB(const MachineInstr &Br) {
  for (unsigned I = 0, E = Br.getDesc().getNumOperands(); I < E; ++I) {
    const MachineOperand &MO = Br.getOperand(I);

    if (MO.isMBB())
      return MO.getMBB();
  }

  llvm_unreachable("This instruction does not have an MBB operand.");
}

// Traverse the list of instructions backwards until a non-debug instruction is
// found or it reaches E.
static ReverseIter getNonDebugInstr(ReverseIter B, const ReverseIter &E) {
  for (; B != E; ++B)
    if (!B->isDebugInstr())
      return B;

  return E;
}

// Split MBB if it has two direct jumps/branches.
void Cpu0BranchExpansion::splitMBB(MachineBasicBlock *MBB) {
  ReverseIter End = MBB->rend();
  ReverseIter LastBr = getNonDebugInstr(MBB->rbegin(), End);

  // Return if MBB has no branch instructions.
  if ((LastBr == End) ||
      (!LastBr->isConditionalBranch() && !LastBr->isUnconditionalBranch()))
    return;

  ReverseIter FirstBr = getNonDebugInstr(std::next(LastBr), End);

  // MBB has only one branch instruction if FirstBr is not a branch
  // instruction.
  if ((FirstBr == End) ||
      (!FirstBr->isConditionalBranch() && !FirstBr->isUnconditionalBranch()))
    return;

  assert(!FirstBr->isIndirectBranch() && "Unexpected indirect branch found.");

  // Create a new MBB. Move instructions in MBB to the newly created MBB.
  MachineBasicBlock *NewMBB =
      MFp->CreateMachineBasicBlock(MBB->getBasicBlock());

  // Insert NewMBB and fix control flow.
  MachineBasicBlock *Tgt = getTargetMBB(*FirstBr);
  NewMBB->transferSuccessors(MBB);
  if (Tgt != getTargetMBB(*LastBr))
    NewMBB->removeSuccessor(Tgt, true);
  MBB->addSuccessor(NewMBB);
  MBB->addSuccessor(Tgt);
  MFp->insert(std::next(MachineFunction::iterator(MBB)), NewMBB);

  NewMBB->splice(NewMBB->end(), MBB, LastBr.getReverse(), MBB->end());
}

// Fill MBBInfos.
void Cpu0BranchExpansion::initMBBInfo() {
  // Split the MBBs if they have two branches. Each basic block should have at
  // most one branch after this loop is executed.
  for (auto &MBB : *MFp)
    splitMBB(&MBB);

  MFp->RenumberBlocks();
  MBBInfos.clear();
  MBBInfos.resize(MFp->size());

  for (unsigned I = 0, E = MBBInfos.size(); I < E; ++I) {
    MachineBasicBlock *MBB = MFp->getBlockNumbered(I);

    // Compute size of MBB.
    for (MachineBasicBlock::instr_iterator MI = MBB->instr_begin();
         MI != MBB->instr_end(); ++MI)
      MBBInfos[I].Size += TII->getInstSizeInBytes(*MI);
  }
}

// Compute offset of branch in number of bytes.
int64_t Cpu0BranchExpansion::computeOffset(const MachineInstr *Br) {
  int64_t Offset = 0;
  int ThisMBB = Br->getParent()->getNumber();
  int TargetMBB = getTargetMBB(*Br)->getNumber();

  // Compute offset of a forward branch.
  if (ThisMBB < TargetMBB) {
    for (int N = ThisMBB + 1; N < TargetMBB; ++N)
      Offset += MBBInfos[N].Size;

    return Offset + 4;
  }

  // Compute offset of a backward branch.
  for (int N = ThisMBB; N >= TargetMBB; --N)
    Offset += MBBInfos[N].Size;

  return -Offset + 4;
}

// Returns the distance in bytes up until MBB
uint64_t Cpu0BranchExpansion::computeOffsetFromTheBeginning(int MBB) {
  uint64_t Offset = 0;
  for (int N = 0; N < MBB; ++N)
    Offset += MBBInfos[N].Size;
  return Offset;
}

// Replace Br with a branch which has the opposite condition code and a
// MachineBasicBlock operand MBBOpnd.
void Cpu0BranchExpansion::replaceBranch(MachineBasicBlock &MBB, Iter Br,
                                        const DebugLoc &DL,
                                        MachineBasicBlock *MBBOpnd) {
  unsigned NewOpc = TII->getOppositeBranchOpc(Br->getOpcode());
  const MCInstrDesc &NewDesc = TII->get(NewOpc);

  MachineInstrBuilder MIB = BuildMI(MBB, Br, DL, NewDesc);

  for (unsigned I = 0, E = Br->getDesc().getNumOperands(); I < E; ++I) {
    MachineOperand &MO = Br->getOperand(I);

    if (!MO.isReg()) {
      assert(MO.isMBB() && "MBB operand expected.");
      break;
    }

    MIB.addReg(MO.getReg());
  }

  MIB.addMBB(MBBOpnd);

  if (Br->hasDelaySlot()) {
    // Bundle the instruction in the delay slot to the newly created branch
    // and erase the original branch.
    assert(Br->isBundledWithSucc());
    MachineBasicBlock::instr_iterator II = Br.getInstrIterator();
    MIBundleBuilder(&*MIB).append((++II)->removeFromBundle());
  }
  Br->eraseFromParent();
}

// Expand branch instructions to long branches.
// TODO: This function has to be fixed for beqz16 and bnez16, because it
// currently assumes that all branches have 16-bit offsets, and will produce
// wrong code if branches whose allowed offsets are [-128, -126, ..., 126]
// are present.
void Cpu0BranchExpansion::expandToLongBranch(MBBInfo &I) {
  MachineBasicBlock::iterator Pos;
  MachineBasicBlock *MBB = I.Br->getParent(), *TgtMBB = getTargetMBB(*I.Br);
  DebugLoc DL = I.Br->getDebugLoc();
  const BasicBlock *BB = MBB->getBasicBlock();
  MachineFunction::iterator FallThroughMBB = ++MachineFunction::iterator(MBB);
  MachineBasicBlock *LongBrMBB = MFp->CreateMachineBasicBlock(BB);

  MFp->insert(FallThroughMBB, LongBrMBB);
  MBB->replaceSuccessor(TgtMBB, LongBrMBB);

  if (IsPIC) {
    MachineBasicBlock *BalTgtMBB = MFp->CreateMachineBasicBlock(BB);
    MFp->insert(FallThroughMBB, BalTgtMBB);
    LongBrMBB->addSuccessor(BalTgtMBB);
    BalTgtMBB->addSuccessor(TgtMBB);

    // We must select between the MIPS32r6/MIPS64r6 BALC (which is a normal
    // instruction) and the pre-MIPS32r6/MIPS64r6 definition (which is an
    // pseudo-instruction wrapping BGEZAL).
    const unsigned BalOp = Cpu0::BAL;

    // $longbr:
    //  addiu $sp, $sp, -8
    //  st $lr, 0($sp)
    //  lui $at, %hi($tgt - $baltgt)
    //  addiu $lr, $lr, %lo($tgt - $baltgt)
    //  bal $baltgt
    //  nop
    // $baltgt:
    //  addu $at, $lr, $at
    //  addiu $sp, $sp, 8
    //  ld $lr, 0($sp)
    //  jr $at
    //  nop
    // $fallthrough:
    //

    Pos = LongBrMBB->begin();

    BuildMI(*LongBrMBB, Pos, DL, TII->get(Cpu0::ADDiu), Cpu0::SP)
        .addReg(Cpu0::SP)
        .addImm(-8);
    BuildMI(*LongBrMBB, Pos, DL, TII->get(Cpu0::ST))
        .addReg(Cpu0::LR)
        .addReg(Cpu0::SP)
        .addImm(0);

    // LUi and ADDiu instructions create 32-bit offset of the target basic
    // block from the target of BAL(C) instruction.  We cannot use immediate
    // value for this offset because it cannot be determined accurately when
    // the program has inline assembly statements.  We therefore use the
    // relocation expressions %hi($tgt-$baltgt) and %lo($tgt-$baltgt) which
    // are resolved during the fixup, so the values will always be correct.
    //
    // Since we cannot create %hi($tgt-$baltgt) and %lo($tgt-$baltgt)
    // expressions at this point (it is possible only at the MC layer),
    // we replace LUi and ADDiu with pseudo instructions
    // LONG_BRANCH_LUi and LONG_BRANCH_ADDiu, and add both basic
    // blocks as operands to these instructions.  When lowering these pseudo
    // instructions to LUi and ADDiu in the MC layer, we will create
    // %hi($tgt-$baltgt) and %lo($tgt-$baltgt) expressions and add them as
    // operands to lowered instructions.

    BuildMI(*LongBrMBB, Pos, DL, TII->get(Cpu0::LONG_BRANCH_LUi), Cpu0::AT)
        .addMBB(TgtMBB)
        .addMBB(BalTgtMBB);
    BuildMI(*LongBrMBB, Pos, DL, TII->get(Cpu0::LONG_BRANCH_ADDiu), Cpu0::AT)
        .addReg(Cpu0::AT)
        .addMBB(TgtMBB)
        .addMBB(BalTgtMBB);
    MIBundleBuilder(*LongBrMBB, Pos)
        .append(BuildMI(*MFp, DL, TII->get(BalOp)).addMBB(BalTgtMBB));

    Pos = BalTgtMBB->begin();

    BuildMI(*BalTgtMBB, Pos, DL, TII->get(Cpu0::ADDu), Cpu0::AT)
        .addReg(Cpu0::LR)
        .addReg(Cpu0::AT);
    BuildMI(*BalTgtMBB, Pos, DL, TII->get(Cpu0::LD), Cpu0::LR)
        .addReg(Cpu0::SP)
        .addImm(0);
    BuildMI(*BalTgtMBB, Pos, DL, TII->get(Cpu0::ADDiu), Cpu0::SP)
        .addReg(Cpu0::SP)
        .addImm(8);

    MIBundleBuilder(*BalTgtMBB, Pos)
        .append(BuildMI(*MFp, DL, TII->get(Cpu0::JR)).addReg(Cpu0::AT))
        .append(BuildMI(*MFp, DL, TII->get(Cpu0::NOP)));

    assert(LongBrMBB->size() + BalTgtMBB->size() == LongBranchSeqSize);
  } else { // Not PIC
    // $longbr:
    //  jmp $tgt
    //  nop
    // $fallthrough:
    //
    Pos = LongBrMBB->begin();
    LongBrMBB->addSuccessor(TgtMBB);

    Pos = LongBrMBB->begin();
    LongBrMBB->addSuccessor(TgtMBB);
    MIBundleBuilder(*LongBrMBB, Pos)
        .append(BuildMI(*MFp, DL, TII->get(Cpu0::JMP)).addMBB(TgtMBB))
        .append(BuildMI(*MFp, DL, TII->get(Cpu0::NOP)));

    assert(LongBrMBB->size() == LongBranchSeqSize);
  }

  if (I.Br->isUnconditionalBranch()) {
    // Change branch destination.
    assert(I.Br->getDesc().getNumOperands() == 1);
    I.Br->removeOperand(0);
    I.Br->addOperand(MachineOperand::CreateMBB(LongBrMBB));
  } else
    // Change branch destination and reverse condition.
    replaceBranch(*MBB, I.Br, DL, &*FallThroughMBB);
}

static void emitGPDisp(MachineFunction &MF, const Cpu0InstrInfo *TII) {
  MachineBasicBlock &MBB = MF.front();
  MachineBasicBlock::iterator I = MBB.begin();
  DebugLoc DL = MBB.findDebugLoc(MBB.begin());
  BuildMI(MBB, I, DL, TII->get(Cpu0::LUi), Cpu0::V0)
      .addExternalSymbol("_gp_disp", Cpu0II::MO_ABS_HI);
  BuildMI(MBB, I, DL, TII->get(Cpu0::ADDiu), Cpu0::V0)
      .addReg(Cpu0::V0)
      .addExternalSymbol("_gp_disp", Cpu0II::MO_ABS_LO);
  MBB.removeLiveIn(Cpu0::V0);
}

bool Cpu0BranchExpansion::runOnMachineFunction(MachineFunction &MF) {
  const TargetMachine &TM = MF.getTarget();
  IsPIC = TM.isPositionIndependent();
  ABI = static_cast<const Cpu0TargetMachine &>(TM).getABI();
  STI = &MF.getSubtarget<Cpu0Subtarget>();
  TII = static_cast<const Cpu0InstrInfo *>(STI->getInstrInfo());
  LongBranchSeqSize = !IsPIC ? 2 : 10;

  if (!STI->enableLongBranchPass())
    return false;

  if (IsPIC && static_cast<const Cpu0TargetMachine &>(TM).getABI().IsO32() &&
      MF.getInfo<Cpu0FunctionInfo>()->globalBaseRegSet())
    emitGPDisp(MF, TII);

  MFp = &MF;
  initMBBInfo();

  SmallVectorImpl<MBBInfo>::iterator I, E = MBBInfos.end();
  bool EverMadeChange = false, MadeChange = true;

  while (MadeChange) {
    MadeChange = false;

    for (I = MBBInfos.begin(); I != E; ++I) {
      // Skip if this MBB doesn't have a branch or the branch has already been
      // converted to a long branch.
      if (!I->Br || I->HasLongBranch)
        continue;

      int ShVal = 4;
      int64_t Offset = computeOffset(I->Br) / ShVal;

      // Check if offset fits into 16-bit immediate field of branches.
      if (!ForceLongBranch && isInt<16>(Offset))
        continue;

      I->HasLongBranch = true;
      I->Size += LongBranchSeqSize * 4;
      ++LongBranches;
      EverMadeChange = MadeChange = true;
    }
  }

  if (!EverMadeChange)
    return true;

  // Compute basic block addresses.
  if (IsPIC) {
    uint64_t Offset = 0;

    for (I = MBBInfos.begin(); I != E; Offset += I->Size, ++I)
      I->Offset = Offset;
  }

  // Do the expansion.
  for (I = MBBInfos.begin(); I != E; ++I)
    if (I->HasLongBranch)
      expandToLongBranch(*I);

  MFp->RenumberBlocks();

  return true;
}
