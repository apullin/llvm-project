set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Analysis
  CodeGen
  Core
  IPO
  AggressiveInstCombine
  InstCombine
  Instrumentation
  MC
  MCParser
  ObjCARCOpts
  Option
  ScalarOpts
  Support
  TargetParser
  TransformUtils
  Vectorize
  )

# Support plugins.
if(CLANG_PLUGIN_SUPPORT)
  set(support_plugins SUPPORT_PLUGINS)
endif()

add_clang_tool(clang
  driver.cpp
  cc1_main.cpp
  cc1as_main.cpp
  cc1gen_reproducer_main.cpp

  DEPENDS
  intrinsics_gen
  # These generated headers are included transitively.
  ARMTargetParserTableGen
  AArch64TargetParserTableGen
  ${support_plugins}
  GENERATE_DRIVER
  )

clang_target_link_libraries(clang
  PRIVATE
  clangBasic
  clangCodeGen
  clangDriver
  clangFrontend
  clangFrontendTool
  clangSerialization
  )

if(WIN32 AND NOT CYGWIN)
  # Prevent versioning if the buildhost is targeting for Win32.
else()
  set_target_properties(clang PROPERTIES VERSION ${CLANG_EXECUTABLE_VERSION})
endif()

# Support plugins.
if(CLANG_PLUGIN_SUPPORT)
  export_executable_symbols_for_plugins(clang)
endif()

add_dependencies(clang clang-resource-headers)

include(GetClangResourceDir)
get_clang_resource_dir(I8085_RESOURCE_DIR PREFIX ${LLVM_LIBRARY_OUTPUT_INTDIR}/..)
set(I8085_LD_SRC "${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085.ld")
set(I8085_LD_DST "${I8085_RESOURCE_DIR}/i8085.ld")
add_custom_command(
  OUTPUT ${I8085_LD_DST}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${I8085_RESOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${I8085_LD_SRC} ${I8085_LD_DST}
  DEPENDS ${I8085_LD_SRC}
  COMMENT "Copying i8085 linker script to Clang resource dir"
  VERBATIM)
add_custom_target(clang-i8085-ldscript DEPENDS ${I8085_LD_DST})
add_dependencies(clang clang-i8085-ldscript)

set(I8085_SYSROOT_DIR "${I8085_RESOURCE_DIR}/i8085")
set(I8085_SYSROOT_LIB_DIR "${I8085_SYSROOT_DIR}/lib")
set(I8085_SYSROOT_INC_DIR "${I8085_SYSROOT_DIR}/include")

set(I8085_CRT_SOURCES
  "${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crt1.s"
  "${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crti.s"
  "${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crtn.s"
  "${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crtbegin.s"
  "${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crtend.s"
  "${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-libc.s"
)

set(I8085_CRT_OBJECTS
  "${I8085_SYSROOT_LIB_DIR}/crt1.o"
  "${I8085_SYSROOT_LIB_DIR}/crti.o"
  "${I8085_SYSROOT_LIB_DIR}/crtn.o"
  "${I8085_SYSROOT_LIB_DIR}/crtbegin.o"
  "${I8085_SYSROOT_LIB_DIR}/crtend.o"
  "${I8085_SYSROOT_LIB_DIR}/libc.o"
  "${I8085_SYSROOT_LIB_DIR}/empty.o"
)

set(I8085_BUILTINS_DIR "${LLVM_MAIN_SRC_DIR}/../compiler-rt/lib/builtins")
set(I8085_LIBGCC_SOURCES
  "${I8085_BUILTINS_DIR}/i8085/int_mul_div.c"
  "${I8085_BUILTINS_DIR}/muldi3.c"
  "${I8085_BUILTINS_DIR}/divdi3.c"
  "${I8085_BUILTINS_DIR}/udivdi3.c"
  "${I8085_BUILTINS_DIR}/moddi3.c"
  "${I8085_BUILTINS_DIR}/umoddi3.c"
  "${I8085_BUILTINS_DIR}/udivmoddi4.c"
  "${I8085_BUILTINS_DIR}/ashldi3.c"
  "${I8085_BUILTINS_DIR}/ashrdi3.c"
  "${I8085_BUILTINS_DIR}/lshrdi3.c"
  "${I8085_BUILTINS_DIR}/clzsi2.c"
  "${I8085_BUILTINS_DIR}/ctzsi2.c"
  "${I8085_BUILTINS_DIR}/clzdi2.c"
  "${I8085_BUILTINS_DIR}/ctzdi2.c"
  "${I8085_BUILTINS_DIR}/cmpdi2.c"
  "${I8085_BUILTINS_DIR}/ucmpdi2.c"
  "${I8085_BUILTINS_DIR}/negdi2.c"
)

set(I8085_LIBGCC_OBJ_DIR "${I8085_SYSROOT_LIB_DIR}/builtins")
set(I8085_LIBGCC_OBJECTS "")
foreach(src ${I8085_LIBGCC_SOURCES})
  get_filename_component(base "${src}" NAME_WE)
  list(APPEND I8085_LIBGCC_OBJECTS "${I8085_LIBGCC_OBJ_DIR}/${base}.o")
endforeach()

set(I8085_LIBGCC_CFLAGS
  --target=i8085-unknown-elf
  -Os
  -ffreestanding
  -fno-builtin
  -fno-stack-protector
  -I${I8085_BUILTINS_DIR}
)

add_custom_command(
  OUTPUT ${I8085_SYSROOT_INC_DIR}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${I8085_SYSROOT_INC_DIR}
  COMMENT "Creating i8085 sysroot include directory"
  VERBATIM)

add_custom_command(
  OUTPUT ${I8085_SYSROOT_LIB_DIR}/crt1.o
  COMMAND ${CMAKE_COMMAND} -E make_directory ${I8085_SYSROOT_LIB_DIR}
  COMMAND ${LLVM_RUNTIME_OUTPUT_INTDIR}/llvm-mc -triple=i8085-unknown-elf -filetype=obj
          ${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crt1.s
          -o ${I8085_SYSROOT_LIB_DIR}/crt1.o
  DEPENDS ${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crt1.s llvm-mc)

add_custom_command(
  OUTPUT ${I8085_SYSROOT_LIB_DIR}/crti.o
  COMMAND ${CMAKE_COMMAND} -E make_directory ${I8085_SYSROOT_LIB_DIR}
  COMMAND ${LLVM_RUNTIME_OUTPUT_INTDIR}/llvm-mc -triple=i8085-unknown-elf -filetype=obj
          ${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crti.s
          -o ${I8085_SYSROOT_LIB_DIR}/crti.o
  DEPENDS ${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crti.s llvm-mc)

add_custom_command(
  OUTPUT ${I8085_SYSROOT_LIB_DIR}/crtn.o
  COMMAND ${CMAKE_COMMAND} -E make_directory ${I8085_SYSROOT_LIB_DIR}
  COMMAND ${LLVM_RUNTIME_OUTPUT_INTDIR}/llvm-mc -triple=i8085-unknown-elf -filetype=obj
          ${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crtn.s
          -o ${I8085_SYSROOT_LIB_DIR}/crtn.o
  DEPENDS ${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crtn.s llvm-mc)

add_custom_command(
  OUTPUT ${I8085_SYSROOT_LIB_DIR}/crtbegin.o
  COMMAND ${CMAKE_COMMAND} -E make_directory ${I8085_SYSROOT_LIB_DIR}
  COMMAND ${LLVM_RUNTIME_OUTPUT_INTDIR}/llvm-mc -triple=i8085-unknown-elf -filetype=obj
          ${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crtbegin.s
          -o ${I8085_SYSROOT_LIB_DIR}/crtbegin.o
  DEPENDS ${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crtbegin.s llvm-mc)

add_custom_command(
  OUTPUT ${I8085_SYSROOT_LIB_DIR}/crtend.o
  COMMAND ${CMAKE_COMMAND} -E make_directory ${I8085_SYSROOT_LIB_DIR}
  COMMAND ${LLVM_RUNTIME_OUTPUT_INTDIR}/llvm-mc -triple=i8085-unknown-elf -filetype=obj
          ${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crtend.s
          -o ${I8085_SYSROOT_LIB_DIR}/crtend.o
  DEPENDS ${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crtend.s llvm-mc)

add_custom_command(
  OUTPUT ${I8085_SYSROOT_LIB_DIR}/libc.o
  COMMAND ${CMAKE_COMMAND} -E make_directory ${I8085_SYSROOT_LIB_DIR}
  COMMAND ${LLVM_RUNTIME_OUTPUT_INTDIR}/llvm-mc -triple=i8085-unknown-elf -filetype=obj
          ${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-libc.s
          -o ${I8085_SYSROOT_LIB_DIR}/libc.o
  DEPENDS ${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-libc.s llvm-mc)

add_custom_command(
  OUTPUT ${I8085_SYSROOT_LIB_DIR}/empty.o
  COMMAND ${CMAKE_COMMAND} -E make_directory ${I8085_SYSROOT_LIB_DIR}
  COMMAND ${LLVM_RUNTIME_OUTPUT_INTDIR}/llvm-mc -triple=i8085-unknown-elf -filetype=obj
          ${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crtend.s
          -o ${I8085_SYSROOT_LIB_DIR}/empty.o
  DEPENDS ${CLANG_SOURCE_DIR}/lib/Driver/ToolChains/i8085-crtend.s llvm-mc)

add_custom_command(
  OUTPUT ${I8085_SYSROOT_LIB_DIR}/libc.a
  COMMAND ${LLVM_RUNTIME_OUTPUT_INTDIR}/llvm-ar rcs ${I8085_SYSROOT_LIB_DIR}/libc.a
          ${I8085_SYSROOT_LIB_DIR}/libc.o
  DEPENDS ${I8085_SYSROOT_LIB_DIR}/libc.o llvm-ar)

add_custom_command(
  OUTPUT ${I8085_SYSROOT_LIB_DIR}/libgcc.a
  COMMAND ${LLVM_RUNTIME_OUTPUT_INTDIR}/llvm-ar rcs ${I8085_SYSROOT_LIB_DIR}/libgcc.a
          ${I8085_SYSROOT_LIB_DIR}/empty.o
  DEPENDS ${I8085_SYSROOT_LIB_DIR}/empty.o llvm-ar)

add_custom_target(clang-i8085-sysroot
  DEPENDS
    ${I8085_SYSROOT_INC_DIR}
    ${I8085_SYSROOT_LIB_DIR}/libc.a
    ${I8085_SYSROOT_LIB_DIR}/libgcc.a
    ${I8085_CRT_OBJECTS})

add_dependencies(clang clang-i8085-sysroot)

add_custom_command(
  TARGET clang POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${I8085_LIBGCC_OBJ_DIR}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${I8085_SYSROOT_LIB_DIR}
  COMMAND ${CMAKE_COMMAND} -E rm -f ${I8085_SYSROOT_LIB_DIR}/libgcc.a
  COMMENT "Preparing i8085 libgcc build"
  VERBATIM)

foreach(src ${I8085_LIBGCC_SOURCES})
  get_filename_component(base "${src}" NAME_WE)
  add_custom_command(
    TARGET clang POST_BUILD
    COMMAND $<TARGET_FILE:clang> ${I8085_LIBGCC_CFLAGS} -c ${src}
            -o ${I8085_LIBGCC_OBJ_DIR}/${base}.o
    VERBATIM)
endforeach()

add_custom_command(
  TARGET clang POST_BUILD
  COMMAND ${LLVM_RUNTIME_OUTPUT_INTDIR}/llvm-ar rcs
          ${I8085_SYSROOT_LIB_DIR}/libgcc.a ${I8085_LIBGCC_OBJECTS}
  COMMENT "Building i8085 libgcc from compiler-rt builtins"
  VERBATIM)

if(NOT CLANG_LINKS_TO_CREATE)
  set(CLANG_LINKS_TO_CREATE clang++ clang-cl clang-cpp)
endif()

if (CLANG_ENABLE_HLSL)
  set(HLSL_LINK clang-dxc)
endif()

foreach(link ${CLANG_LINKS_TO_CREATE} ${HLSL_LINK})
  add_clang_symlink(${link} clang)
endforeach()

# Configure plist creation for OS X.
set (TOOL_INFO_PLIST "Info.plist" CACHE STRING "Plist name")
if (APPLE)
  if (CLANG_VENDOR)
    set(TOOL_INFO_NAME "${CLANG_VENDOR} clang")
  else()
    set(TOOL_INFO_NAME "clang")
  endif()

  set(TOOL_INFO_UTI "${CLANG_VENDOR_UTI}")
  set(TOOL_INFO_VERSION "${CLANG_VERSION}")
  set(TOOL_INFO_BUILD_VERSION "${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}")

  set(TOOL_INFO_PLIST_OUT "${CMAKE_CURRENT_BINARY_DIR}/${TOOL_INFO_PLIST}")

  if(LLVM_TOOL_LLVM_DRIVER_BUILD AND clang IN_LIST LLVM_DRIVER_TOOLS)
    set(TARGET_NAME llvm-driver)
  else()
    set(TARGET_NAME clang)
  endif()

  target_link_libraries(${TARGET_NAME}
    PRIVATE
    "-Wl,-sectcreate,__TEXT,__info_plist,\"${TOOL_INFO_PLIST_OUT}\"")
  configure_file("${TOOL_INFO_PLIST}.in" "${TOOL_INFO_PLIST_OUT}" @ONLY)

  set(TOOL_INFO_UTI)
  set(TOOL_INFO_NAME)
  set(TOOL_INFO_VERSION)
  set(TOOL_INFO_BUILD_VERSION)
endif()

if(CLANG_ORDER_FILE AND
    (LLVM_LINKER_IS_APPLE OR LLVM_LINKER_IS_GOLD OR LLVM_LINKER_IS_LLD))
  include(LLVMCheckLinkerFlag)

  if (LLVM_LINKER_IS_APPLE OR (LLVM_LINKER_IS_LLD AND APPLE))
    set(LINKER_ORDER_FILE_OPTION "-Wl,-order_file,${CLANG_ORDER_FILE}")
  elseif (LLVM_LINKER_IS_GOLD)
    set(LINKER_ORDER_FILE_OPTION "-Wl,--section-ordering-file,${CLANG_ORDER_FILE}")
  elseif (LLVM_LINKER_IS_LLD)
    set(LINKER_ORDER_FILE_OPTION "-Wl,--symbol-ordering-file,${CLANG_ORDER_FILE}")
  endif()

  # This is a test to ensure the actual order file works with the linker.
  llvm_check_linker_flag(CXX ${LINKER_ORDER_FILE_OPTION} LINKER_ORDER_FILE_WORKS)

  # Passing an empty order file disables some linker layout optimizations.
  # To work around this and enable workflows for re-linking when the order file
  # changes we check during configuration if the file is empty, and make it a
  # configuration dependency.
  file(READ ${CLANG_ORDER_FILE} ORDER_FILE LIMIT 20)
  if("${ORDER_FILE}" STREQUAL "\n")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CLANG_ORDER_FILE})
  elseif(LINKER_ORDER_FILE_WORKS)
    target_link_libraries(clang PRIVATE ${LINKER_ORDER_FILE_OPTION})
    set_target_properties(clang PROPERTIES LINK_DEPENDS ${CLANG_ORDER_FILE})
  endif()
endif()
