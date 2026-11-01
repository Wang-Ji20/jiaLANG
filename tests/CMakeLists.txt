add_subdirectory(unit)

add_executable(jia-lit lit/jia-lit.cpp)
target_compile_features(jia-lit PRIVATE cxx_std_23)

set(JIA_LIT_COMMAND jia-lit --jiac $<TARGET_FILE:jiac> --cc ${CMAKE_C_COMPILER}
    --out ${CMAKE_CURRENT_BINARY_DIR}/Output)
add_test(NAME check COMMAND ${JIA_LIT_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/check)
add_test(NAME e2e COMMAND ${JIA_LIT_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/e2e)
