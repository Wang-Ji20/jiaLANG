cmake_minimum_required(VERSION 3.20)
project(jialang VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

option(JIA_WERROR "Treat compiler warnings as errors" OFF)
option(JIA_SANITIZE "Build with AddressSanitizer and UBSan" OFF)

find_package(LLVM REQUIRED CONFIG)
if(LLVM_PACKAGE_VERSION VERSION_LESS 21)
  message(FATAL_ERROR "jiaLang needs LLVM 21 or newer, found ${LLVM_PACKAGE_VERSION}")
endif()
message(STATUS "Using LLVM ${LLVM_PACKAGE_VERSION} from ${LLVM_DIR}")

# Flags shared by every jiaLang target. LLVM headers are SYSTEM includes so
# their warnings do not drown ours.
add_library(jia_options INTERFACE)
target_include_directories(jia_options INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(jia_options SYSTEM INTERFACE ${LLVM_INCLUDE_DIRS})
separate_arguments(_llvm_defs NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(jia_options INTERFACE ${_llvm_defs})
target_compile_options(jia_options INTERFACE
  -Wall -Wextra -Wpedantic -Wshadow -Wno-missing-field-initializers -fno-exceptions
  $<$<BOOL:${JIA_WERROR}>:-Werror>)
if(JIA_SANITIZE)
  target_compile_options(jia_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(jia_options INTERFACE -fsanitize=address,undefined)
endif()

if(LLVM_LINK_LLVM_DYLIB)
  set(JIA_LLVM_LIBS LLVM)
else()
  llvm_map_components_to_libnames(JIA_LLVM_LIBS
    core support passes analysis target mc bitwriter ${LLVM_TARGETS_TO_BUILD})
endif()

# jia_add_library(<Name> SOURCES... DEPENDS...) creates library jia<Name>.
function(jia_add_library name)
  cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})
  add_library(jia${name} STATIC ${ARG_SOURCES})
  target_link_libraries(jia${name} PUBLIC jia_options ${ARG_DEPENDS})
endfunction()

add_subdirectory(lib)
add_subdirectory(tools)

enable_testing()
add_subdirectory(tests)
