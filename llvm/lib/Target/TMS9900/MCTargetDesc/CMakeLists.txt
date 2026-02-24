add_llvm_component_library(LLVMTMS9900Desc
  TMS9900AsmBackend.cpp
  TMS9900ELFObjectWriter.cpp
  TMS9900MCCodeEmitter.cpp
  TMS9900MCTargetDesc.cpp

  LINK_COMPONENTS
  MC
  TMS9900Info
  Support

  ADD_TO_COMPONENT
  TMS9900

  DEPENDS
  TMS9900CommonTableGen
  )
