#include "AST.hh"

void print_tab(int n)
{
    for (int i = 0; i < n; i++)
        printf("  ");
}

void my_visit(std::shared_ptr<AST> ast, int layer, bool trailing_comma)
{
    print_tab(layer);
    std::cout << "{" << std::endl;
    print_tab(layer + 1);
    std::cout << "\"Node Name\": \"" << ast->name << "\"," << std::endl;

    if (ast->children.empty())
    {
        print_tab(layer + 1);
        std::cout << "\"Children\": []" << std::endl;
    }
    else
    {
        print_tab(layer + 1);
        std::cout << "\"Children\": [" << std::endl;

        for (auto i = ast->children.begin(); i != ast->children.end(); i++)
        {
            if (i + 1 == ast->children.end())
                my_visit(*i, layer + 2, false);
            else
                my_visit(*i, layer + 2, true);
        }

        print_tab(layer + 1);
        std::cout << "]" << std::endl;
    }
    print_tab(layer);
    if (trailing_comma)
        std::cout << "}," << std::endl;
    else 
        std::cout << "}" << std::endl;
}

void PrintVisitor::visit(std::shared_ptr<AST> ast)
{
    my_visit(ast, 0, false);
}