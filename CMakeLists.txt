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

#add_definitions(-D__KERNEL__ -DMODULE )
