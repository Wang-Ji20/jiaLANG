add_executable(jia-unittests main.cpp BasicTest.cpp LexerTest.cpp ASTTest.cpp ParserTest.cpp)
target_link_libraries(jia-unittests PRIVATE jiaParse)
add_test(NAME unit COMMAND jia-unittests)
