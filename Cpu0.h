// Copyright 2022 All Rights Reserved.
// Author: lanzongwei541@gmail.com (lanzongwei)

#ifndef LLVM_LIB_TARGET_CPU0_CPU0_H
#define LLVM_LIB_TARGET_CPU0_CPU0_H

#include "llvm/Support/CodeGen.h"

namespace llvm {
  class Cpu0TargetMachine;
  class FunctionPass;
}

#define ENABLE_GPRESTORE  // The $gp register caller saved register enable

#endif
