add_llvm_component_library(LLVMCpu0Desc
  Cpu0MCTargetDesc.cpp

  LINK_COMPONENTS
  MC
  Support
  Cpu0Info

  ADD_TO_COMPONENT
  Cpu0
  )
