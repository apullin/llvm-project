add_llvm_component_library(LLVMTMS9900Info
  TMS9900TargetInfo.cpp

  LINK_COMPONENTS
  Support

  ADD_TO_COMPONENT
  TMS9900
  )
