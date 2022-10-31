cmake_minimum_required(VERSION 3.22.2)
project(ktest_hello)

set(kerneldir "" CACHE STRING "Path to the kernel build directory")
if("${kerneldir}" STREQUAL "")
  execute_process(COMMAND uname -r OUTPUT_VARIABLE uname_r
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(kerneldir "/lib/modules/${uname_r}/build")
endif()
find_file(kernel_makefile NAMES Makefile
                          PATHS ${kerneldir} NO_DEFAULT_PATH)
if(NOT kernel_makefile)
  message(FATAL_ERROR "There is no Makefile in kerneldir!")
endif()

#string(REPLACE ";" " " module_sources_string "${module_sources}")
#configure_file(Kbuild.in Kbuild @ONLY)

# file KBuild.in contain:
#obj-m := mymodule.o
#mymodule-objs := $(pathsubst %.c,%.o, $(filter %.c, @module_sources_string@))

set(module_sources ktest_module.c Kbuild)
foreach(src ${module_sources})
  configure_file(${src} ${src} COPYONLY)
endforeach()

set(module_cmd make -C ${kerneldir} M=${CMAKE_CURRENT_BINARY_DIR})
add_custom_command(OUTPUT mymodule.ko
  COMMAND ${module_cmd} modules
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${module_sources} ${CMAKE_CURRENT_BINARY_DIR}/Makefile
  VERBATIM)
add_custom_target(module ALL DEPENDS mymodule.ko)
add_custom_target(module-clean COMMAND ${module_cmd} clean)

#"command": "gcc 
# -Wp,-MMD,/home/sydnash/driver/hello/.ktest_module.o.d -nostdinc 
#  -DKBUILD_BASENAME='\"ktest_module\"' -DKBUILD_MODNAME='\"ktest_hello\"' -D__KBUILD_MODNAME=kmod_ktest_hello
# -c -o /home/sydnash/driver/hello/ktest_module.o /home/sydnash/driver/hello/ktest_module.c",

add_compile_options(-D__KERNEL__ -mlittle-endian -DCC_USING_PATCHABLE_FUNCTION_ENTRY -DKASAN_SHADOW_SCALE_SHIFT= -fmacro-prefix-map=./= 
-Wall -Wundef -Werror=strict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar
-fno-PIE -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Wno-format-security -std=gnu11
-mgeneral-regs-only -DCONFIG_CC_HAS_K_CONSTRAINT=1 -Wno-psabi -mabi=lp64 -fno-asynchronous-unwind-tables
-fno-unwind-tables -mbranch-protection=pac-ret+leaf+bti
-Wa,-march=armv8.5-a -DARM64_ASM_ARCH='\"armv8.5-a\"' -DKASAN_SHADOW_SCALE_SHIFT=
-fno-delete-null-pointer-checks -Wno-frame-address -Wno-format-truncation -Wno-format-overflow -Wno-address-of-packed-member
-O2 -fno-allow-store-data-races -Wframe-larger-than=1024 -fno-stack-protector -Wimplicit-fallthrough=5
-Wno-main -Wno-unused-but-set-variable -Wno-unused-const-variable
-fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-stack-clash-protection
-fpatchable-function-entry=2 -fno-inline-functions-called-once -Wdeclaration-after-statement -Wvla
-Wno-pointer-sign -Wcast-function-type -Wno-stringop-truncation -Wno-stringop-overflow -Wno-restrict
-Wno-maybe-uninitialized -Wno-alloc-size-larger-than -fno-strict-overflow -fno-stack-check -fconserve-stack
-Werror=date-time -Werror=incompatible-pointer-types -Werror=designated-init -Wno-packed-not-aligned -g
 -DMODULE
)
add_definitions(-DKBUILD_BASENAME=\"ktest_module\")
add_compile_definitions(KBUILD_MODNAME=\"ktest_hello\" __KBUILD_MODNAME=kmod_ktest_hello)
add_compile_options(
  -include ${kerneldir}/include/linux/compiler-version.h
  -include ${kerneldir}/include/linux/kconfig.h
  -include ${kerneldir}/include/linux/compiler_types.h
)
file(GLOB srcs *.c)
add_library(fake_lib STATIC EXCLUDE_FROM_ALL ${srcs})
target_include_directories(fake_lib PRIVATE
  ${kerneldir}/arch/arm64/include
  ${kerneldir}/arch/arm64/include/generated
  ${kerneldir}/include
  ${kerneldir}/arch/arm64/include/uapi 
  ${kerneldir}/arch/arm64/include/generated/uapi 
  ${kerneldir}/include/uapi
  ${kerneldir}/include/generated/uapi 
)
