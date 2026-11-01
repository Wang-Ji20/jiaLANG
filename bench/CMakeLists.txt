add_executable(jia-bench jia-bench.cpp)
target_compile_features(jia-bench PRIVATE cxx_std_23)

file(GLOB JIA_BENCH_PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/programs/*.jia)
set(JIA_BENCH_COMMAND jia-bench --jiac $<TARGET_FILE:jiac> --cc ${CMAKE_C_COMPILER}
    --out ${CMAKE_CURRENT_BINARY_DIR}/Output)

# `cmake --build build --target bench` runs the full benchmark (slow).
add_custom_target(bench
  COMMAND ${JIA_BENCH_COMMAND} --runs 5 --report ${CMAKE_CURRENT_BINARY_DIR}/report.md
          ${JIA_BENCH_PROGRAMS}
  DEPENDS jia-bench jiac
  USES_TERMINAL)

# A one-run smoke test in ctest: every benchmark builds everywhere and all
# configurations print the same output.
add_test(NAME bench-smoke
  COMMAND ${JIA_BENCH_COMMAND} --runs 1 --no-throughput ${JIA_BENCH_PROGRAMS})
