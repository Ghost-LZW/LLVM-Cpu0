// Copyright 2022 All Rights Reserved.
// Author: lanzongwei541@gmail.com (lanzongwei)
//
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//
//
// This file implements the Cpu0AsmBackend class.
//
//===----------------------------------------------------------------------===//
//

#include "Cpu0AsmBackend.h"
#include "Cpu0FixupKinds.h"
#include "Cpu0MCTargetDesc.h"

#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCDirectives.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static cl::opt<bool> HasLLD("has-lld", cl::init(false),
                            cl::desc("CPU0: Has lld linker for Cpu0."),
                            cl::Hidden);

//@adjustFixupValue {
// Prepare value for the target space for it
static unsigned adjustFixupValue(const MCFixup &Fixup, uint64_t Value,
                                 MCContext &Ctx) {

  unsigned Kind = Fixup.getKind();

  // Add/subtract and shift
  switch (Kind) {
  default:
    return 0;
  case FK_GPRel_4:
  case FK_Data_4:
#if 0 // CH >= CH9_1
  case Cpu0::fixup_Cpu0_CALL16:
#endif
  case Cpu0::fixup_Cpu0_LO16:
  case Cpu0::fixup_Cpu0_GOT_LO16:
    break;
  case Cpu0::fixup_Cpu0_PC16:
  case Cpu0::fixup_Cpu0_PC24:
    // So far we are only using this type for branches and jump.
    // For branches we start 1 instruction after the branch
    // so the displacement will be one instruction size less.
    Value -= 4;
    break;
  case Cpu0::fixup_Cpu0_HI16:
  case Cpu0::fixup_Cpu0_GOT:
  case Cpu0::fixup_Cpu0_GOT_HI16:
    // Get the higher 16-bits. Also add 1 if bit 15 is 1.
    Value = (Value >> 16) & 0xffff;
    break;
  }

  return Value;
}
//@adjustFixupValue }

std::unique_ptr<MCObjectTargetWriter>
Cpu0AsmBackend::createObjectTargetWriter() const {
  return createCpu0ELFObjectWriter(TheTriple);
}

/// ApplyFixup - Apply the \p Value for given \p Fixup into the provided
/// data fragment, at the offset specified by the fixup and following the
/// fixup kind as appropriate.
void Cpu0AsmBackend::applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                                const MCValue &Target,
                                MutableArrayRef<char> Data, uint64_t Value,
                                bool IsResolved,
                                const MCSubtargetInfo *STI) const {
  MCFixupKind Kind = Fixup.getKind();
  MCContext &Ctx = Asm.getContext();
  Value = adjustFixupValue(Fixup, Value, Ctx);

  if (!Value)
    return; // Doesn't change encoding.

  // Where do we start in the object
  unsigned Offset = Fixup.getOffset();
  // Number of bytes we need to fixup
  unsigned NumBytes = (getFixupKindInfo(Kind).TargetSize + 7) / 8;
  // Used to point to big endian bytes
  unsigned FullSize;

  switch ((unsigned)Kind) {
  default:
    FullSize = 4;
    break;
  }

  // Grab current value, if any, from bits.
  uint64_t CurVal = 0;

  for (unsigned i = 0; i != NumBytes; ++i) {
    unsigned Idx = TheTriple.isLittleEndian() ? i : (FullSize - 1 - i);
    CurVal |= (uint64_t)((uint8_t)Data[Offset + Idx]) << (i * 8);
  }

  uint64_t Mask = ((uint64_t)(-1) >> (64 - getFixupKindInfo(Kind).TargetSize));
  CurVal |= Value & Mask;

  // Write out the fixed up bytes back to the code/data bits.
  for (unsigned i = 0; i != NumBytes; ++i) {
    unsigned Idx = TheTriple.isLittleEndian() ? i : (FullSize - 1 - i);
    Data[Offset + Idx] = (uint8_t)((CurVal >> (i * 8)) & 0xff);
  }
}

//@getFixupKindInfo {
const MCFixupKindInfo &
Cpu0AsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  unsigned JSUBReloRec = 0;
  if (HasLLD) {
    JSUBReloRec = MCFixupKindInfo::FKF_IsPCRel;
  } else {
    JSUBReloRec = MCFixupKindInfo::FKF_IsPCRel | MCFixupKindInfo::FKF_Constant;
  }
  const static MCFixupKindInfo Infos[Cpu0::NumTargetFixupKinds] = {
    // This table *must* be in same the order of fixup_* kinds in
    // Cpu0FixupKinds.h.
    //
    // name                        offset  bits  flags
    {"fixup_Cpu0_32", 0, 32, 0},
    {"fixup_Cpu0_HI16", 0, 16, 0},
    {"fixup_Cpu0_LO16", 0, 16, 0},
    {"fixup_Cpu0_GPREL16", 0, 16, 0},
    {"fixup_Cpu0_GOT", 0, 16, 0},
    {"fixup_Cpu0_PC16", 0, 16, MCFixupKindInfo::FKF_IsPCRel},
    {"fixup_Cpu0_PC24", 0, 24, JSUBReloRec},
#if 0 // CH >= CH9_1
    { "fixup_Cpu0_CALL16",         0,     16,   0 },
#endif
#if 0 // CH >= CH12_1
    { "fixup_Cpu0_TLSGD",          0,     16,   0 },
    { "fixup_Cpu0_GOTTP",          0,     16,   0 },
    { "fixup_Cpu0_TP_HI",          0,     16,   0 },
    { "fixup_Cpu0_TP_LO",          0,     16,   0 },
    { "fixup_Cpu0_TLSLDM",         0,     16,   0 },
    { "fixup_Cpu0_DTP_HI",         0,     16,   0 },
    { "fixup_Cpu0_DTP_LO",         0,     16,   0 },
#endif
    {"fixup_Cpu0_GOT_HI16", 0, 16, 0},
    {"fixup_Cpu0_GOT_LO16", 0, 16, 0}
  };

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  assert(unsigned(Kind - FirstTargetFixupKind) < getNumFixupKinds() &&
         "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}
//@getFixupKindInfo }

/// WriteNopData - Write an (optimal) nop sequence of Count bytes
/// to the given output. If the target cannot generate such a sequence,
/// it should return an error.
///
/// \return - True on success.
bool Cpu0AsmBackend::writeNopData(raw_ostream &OS, uint64_t Count,
                                  const MCSubtargetInfo *STI) const {
  return true;
}

// MCAsmBackend
MCAsmBackend *llvm::createCpu0AsmBackend(const Target &T,
                                         const MCSubtargetInfo &STI,
                                         const MCRegisterInfo &MRI,
                                         const MCTargetOptions &Options) {
  return new Cpu0AsmBackend(T, STI.getTargetTriple());
}
