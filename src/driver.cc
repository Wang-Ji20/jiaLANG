#include "driver.hh"
#include "parser.hh"

driver::driver() : trace_parsing(false), trace_scanning(false) {
  root = std::make_shared<AST>();
  root->name = "meta";
}

int driver::parse(const std::string &f) {
  file = f;
  location.initialize(&file);
  // 启动 flex，指定文件输入
  scan_begin();
  // 构造 bison parser，bison 通过调用 yylex 获得 token，然后解析。
  // yylex 在 parser 声明，但是在 scanner 实现。
  yy::parser parse(*this);
  parse.set_debug_level(trace_parsing);
  int res = parse();
  scan_end();

  std::cout << "parsing complete\n";
  PrintVisitor pv;
  root->accept(&pv);

  CodegenVisitor &codegen = CodegenVisitor::GetInstance();
  root->accept(&codegen);

  codegen.irOpt();

  codegen.TheModule->print(llvm::errs(), nullptr);

  return res;
}
