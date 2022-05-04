// Copyright 2022 All Rights Reserved.
// Author: lanzongwei541@gmail.com (lanzongwei)
//
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CPU0_CPU0TARGETSTREAMER_H
#define LLVM_LIB_TARGET_CPU0_CPU0TARGETSTREAMER_H

#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/MC/MCELFStreamer.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"

namespace llvm {
class formatted_raw_ostream;

class Cpu0TargetStreamer : public MCTargetStreamer {
public:
  Cpu0TargetStreamer(MCStreamer &S);
};

// This part is for ascii assembly output
class Cpu0TargetAsmStreamer : public Cpu0TargetStreamer {
public:
  Cpu0TargetAsmStreamer(MCStreamer &S);
};

} // namespace llvm

#endif
