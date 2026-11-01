add_executable(jia-unittests main.cpp BasicTest.cpp LexerTest.cpp)
target_link_libraries(jia-unittests PRIVATE jiaLex)
add_test(NAME unit COMMAND jia-unittests)
