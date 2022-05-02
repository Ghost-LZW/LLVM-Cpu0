// Copyright 2022 All Rights Reserved.
// Author: lanzongwei541@gmail.com (lanzongwei)

//===----------------------------------------------------------------------===//
//
// This file declares the Cpu0 specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CPU0_CPU0TARGETMACHINE_H
#define LLVM_LIB_TARGET_CPU0_CPU0TARGETMACHINE_H

#include "Cpu0Subtarget.h"
#include "MCTargetDesc/Cpu0ABIInfo.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Target/TargetMachine.h"
#include <memory>

namespace llvm {

Target &getTheCpu0Target();
Target &getTheCpu0elTarget();

class Cpu0TargetMachine : public LLVMTargetMachine {
  bool isLittle;
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  Cpu0ABIInfo ABI;
  Cpu0Subtarget DefaultSubtarget;

  mutable StringMap<std::unique_ptr<Cpu0Subtarget>> SubtargetMap;

public:
  Cpu0TargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                    StringRef FS, const TargetOptions &Options,
                    Optional<Reloc::Model> RM, Optional<CodeModel::Model> CM,
                    CodeGenOpt::Level OL, bool JIT, bool isLittle);
  ~Cpu0TargetMachine() override;

  const Cpu0Subtarget *getSubtargetImpl() const { return &DefaultSubtarget; }

  // Can use this interface to fetch subtarget
  const Cpu0Subtarget *getSubtargetImpl(const Function &F) const override;

  // Pass Pipeline Configuration
  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }
  bool isLittleEndian() const { return isLittle; }
  const Cpu0ABIInfo &getABI() const { return ABI; }
};

/// Cpu0 big endian target machine.
///
class Cpu0ebTargetMachine : public Cpu0TargetMachine {
  virtual void anchor();

public:
  Cpu0ebTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                      StringRef FS, const TargetOptions &Options,
                      Optional<Reloc::Model> RM, Optional<CodeModel::Model> CM,
                      CodeGenOpt::Level OL, bool JIT);
};

/// Cpu0 little endian target machine.
///
class Cpu0elTargetMachine : public Cpu0TargetMachine {
  virtual void anchor();

public:
  Cpu0elTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                      StringRef FS, const TargetOptions &Options,
                      Optional<Reloc::Model> RM, Optional<CodeModel::Model> CM,
                      CodeGenOpt::Level OL, bool JIT);
};
} // namespace llvm

#endif
