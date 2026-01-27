add_llvm_component_library(LLVMI8085Disassembler
  I8085Disassembler.cpp

  LINK_COMPONENTS
  I8085Info
  MC
  MCDisassembler
  Support

  ADD_TO_COMPONENT
  I8085
  )
