add_executable(jia-unittests main.cpp BasicTest.cpp)
target_link_libraries(jia-unittests PRIVATE jiaBasic)
add_test(NAME unit COMMAND jia-unittests)
