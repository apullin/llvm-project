add_lldb_library(lldbPluginABII8085 PLUGIN
  ABISysV_i8085.cpp

  LINK_LIBS
    lldbCore
    lldbSymbol
    lldbTarget
  LINK_COMPONENTS
    Support
    TargetParser
  )
