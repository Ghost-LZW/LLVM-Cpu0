// Copyright 2022 All Rights Reserved.
// Author: lanzongwei541@gmail.com (lanzongwei)
//
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef LLVM_LIB_TARGET_CPU0_CPU0_H
#define LLVM_LIB_TARGET_CPU0_CPU0_H

#include "MCTargetDesc/Cpu0MCTargetDesc.h"

#include "llvm/CodeGen/Passes.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Target/TargetMachine.h"

#define ENABLE_GPRESTORE

namespace llvm {
class Cpu0TargetMachine;
class FunctionPass;

FunctionPass *createCpu0BranchExpansion();
void initializeCpu0BranchExpansionPass(PassRegistry &);

} // namespace llvm

#endif
