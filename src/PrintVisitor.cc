#include "AST.hh"

void PrintVisitor::visit(std::shared_ptr<AST> ast){
        // visit left child tree
        if(ast->LCT != nullptr)
            ast->LCT->accept(this);

        // visit this node
        std::cout << "i came to a node" << std::endl;
        std::cout << ast->name << std::endl;
        std::cout << "a number: " << ast->value << std::endl;
        std::cout << std::endl;

        // visit right child tree
        if(ast->RCT != nullptr)
            ast->RCT->accept(this);
}