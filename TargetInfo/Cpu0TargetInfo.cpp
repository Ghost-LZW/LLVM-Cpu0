// Copyright 2022 All Rights Reserved.
// Author: lanzongwei541@gmail.com (lanzongwei)

#include "Cpu0TargetInfo.h"

#include "llvm/IR/Module.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

Target &llvm::getTheCpu0Target() {
  static Target TheCpu0Target;
  return TheCpu0Target;
}

Target &llvm::getTheCpu0elTarget() {
  static Target TheCpu0elTarget;
  return TheCpu0elTarget;
}

extern "C" void LLVMInitializeCpu0TargetInfo() {
  RegisterTarget<Triple::cpu0, /*HasJIT=*/true> X(getTheCpu0Target(),
                                                  "cpu0", "Cpu0", "Cpu0");

  RegisterTarget<Triple::cpu0el, /*HasJIT=*/true> Y(getTheCpu0elTarget(),
                                                    "cpu0el", "Cpu0el", "Cpu0");
}
