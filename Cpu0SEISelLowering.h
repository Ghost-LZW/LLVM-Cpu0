// Copyright 2022 All Rights Reserved.
// Author: lanzongwei541@gmail.com (lanzongwei)
//
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef LLVM_LIB_TARGET_CPU0_CPU0SEISELLOWERING_H
#define LLVM_LIB_TARGET_CPU0_CPU0SEISELLOWERING_H

#include "Cpu0ISelLowering.h"

#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/Support/MachineValueType.h"

namespace llvm {
class Cpu0SETargetLowering : public Cpu0TargetLowering {
public:
  explicit Cpu0SETargetLowering(const Cpu0TargetMachine &TM,
                                const Cpu0Subtarget &STI);

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;
};

} // namespace llvm

#endif
