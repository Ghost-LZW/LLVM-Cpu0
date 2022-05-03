add_llvm_component_library(LLVMCpu0Desc
  Cpu0MCTargetDesc.cpp
  Cpu0ABIInfo.cpp
  Cpu0MCAsmInfo.cpp
  Cpu0InstPrinter.cpp
  Cpu0AsmBackend.cpp
  Cpu0MCCodeEmitter.cpp
  Cpu0MCExpr.cpp
  Cpu0ELFObjectWriter.cpp
  Cpu0TargetStreamer.cpp

  LINK_COMPONENTS
  MC
  Support
  Cpu0Info

  ADD_TO_COMPONENT
  Cpu0
  )
