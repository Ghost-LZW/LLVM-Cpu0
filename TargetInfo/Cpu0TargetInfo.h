// Copyright 2022 All Rights Reserved.
// Author: lanzongwei541@gmail.com (lanzongwei)

#ifndef LLVM_LIB_TARGET_Cpu0_TARGETINFO_Cpu0TARGETINFO_H
#define LLVM_LIB_TARGET_Cpu0_TARGETINFO_Cpu0TARGETINFO_H

namespace llvm {

class Target;

Target &getTheCpu0Target();
Target &getTheCpu0elTarget();

}

#endif // LLVM_LIB_TARGET_X86_TARGETINFO_X86TARGETINFO_H
