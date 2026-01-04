add_llvm_component_library(LLVMTMS9900Info
  TMS9900TargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  TMS9900

  DEPENDS
  TMS9900CommonTableGen
  )
