#include "AST.hh"

void PrintVisitor::visit(std::shared_ptr<AST> ast){
    for (int i = 0; i < layer; i++)
    {
        printf("  ");
    }
    printf("- ");
    std::cout << ast->name << std::endl;
    layer++;
    for (auto &&i : ast->children)
    {
        visit(i);
    }
    layer--;
}