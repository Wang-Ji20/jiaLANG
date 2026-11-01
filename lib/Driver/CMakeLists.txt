jia_add_library(Driver SOURCES Options.cpp Backend.cpp Driver.cpp
  DEPENDS jiaParse jiaSema jiaCodeGen ${JIA_LLVM_LIBS})
