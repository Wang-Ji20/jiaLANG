add_executable(jia-unittests main.cpp BasicTest.cpp LexerTest.cpp ASTTest.cpp)
target_link_libraries(jia-unittests PRIVATE jiaLex jiaAST)
add_test(NAME unit COMMAND jia-unittests)
