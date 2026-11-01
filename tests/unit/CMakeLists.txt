add_executable(jia-unittests main.cpp BasicTest.cpp LexerTest.cpp ASTTest.cpp ParserTest.cpp SemaTest.cpp CodeGenTest.cpp)
target_link_libraries(jia-unittests PRIVATE jiaParse jiaSema jiaCodeGen)
add_test(NAME unit COMMAND jia-unittests)
