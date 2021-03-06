// Copyright 2022 All Rights Reserved.
// Author: lanzongwei541@gmail.com (lanzongwei)
//
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "Cpu0MachineFunction.h"

#include "Cpu0InstrInfo.h"
#include "Cpu0Subtarget.h"
#include "MCTargetDesc/Cpu0BaseInfo.h"

#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"

using namespace llvm;

bool FixGlobalBaseReg;

Cpu0FunctionInfo::~Cpu0FunctionInfo() {}

bool Cpu0FunctionInfo::globalBaseRegFixed() const { return FixGlobalBaseReg; }

bool Cpu0FunctionInfo::globalBaseRegSet() const { return GlobalBaseReg; }

unsigned Cpu0FunctionInfo::getGlobalBaseReg() {
  return GlobalBaseReg = Cpu0::GP;
}

void Cpu0FunctionInfo::createEhDataRegsFI() {
  const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
  for (int I = 0; I < 2; ++I) {
    const TargetRegisterClass &RC = Cpu0::CPURegsRegClass;

    EhDataRegFI[I] = MF.getFrameInfo().CreateStackObject(
        TRI.getSpillSize(RC), TRI.getSpillAlign(RC), false);
  }
}

void Cpu0FunctionInfo::anchor() {}
