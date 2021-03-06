// Copyright 2022 All Rights Reserved.
// Author: lanzongwei541@gmail.com (lanzongwei)
//
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//
//
// This file provides Cpu0 specific target streamer methods.
//
//===----------------------------------------------------------------------===//

#include "Cpu0TargetStreamer.h"
#include "Cpu0InstPrinter.h"
#include "Cpu0MCTargetDesc.h"
#include "Cpu0TargetObjectFile.h"

#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;

Cpu0TargetStreamer::Cpu0TargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

Cpu0TargetAsmStreamer::Cpu0TargetAsmStreamer(MCStreamer &S)
    : Cpu0TargetStreamer(S) {}
