#include "AST.hh"
#include "CodegenVisitor.hh"

using namespace std;
using namespace llvm;

// llvm::Value* NumberAST::codegen(CodegenVisitor& v){
//     return v.retv = llvm::ConstantInt::get(*(v.TheContext), llvm::APInt(32, value));
// }

// llvm::Value* MainAST::codegen(CodegenVisitor& v){
    
//     llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getInt32Ty(*v.TheContext), false);

//     llvm::Function* mainf = llvm::Function::Create(ft,
//         llvm::Function::ExternalLinkage, "jialmain", v.TheModule.get());

//     llvm::BasicBlock *BB = llvm::BasicBlock::Create(*v.TheContext, "main", mainf);

//     v.Builder->SetInsertPoint(BB);

//     v.Builder->CreateRet(v.retv);

//     llvm::verifyFunction(*mainf);

//     return mainf;
// }

void
make_function(tnode scope, string type_spec, tnode direct_dcl, tnode param_list, tnode func_body){
    auto func_node = make_shared<FuncDefAST>();
    func_node->Type = type_spec;
    func_node->name += " return type " + type_spec;
    func_node->children.push_back(direct_dcl);
    if(param_list != nullptr)
        func_node->children.push_back(param_list);
    func_node->children.push_back(func_body);
    scope->children.push_back(func_node);
    return;
}

void
make_decl(tnode scope, string type_spec, tnode init_scl_list){
    auto decl_node = make_shared<DeclAST>();
    decl_node->Type = type_spec;
    decl_node->name += " type " + type_spec;
    decl_node->children.push_back(init_scl_list);
    scope->children.push_back(decl_node);
    return;
}

tnode
make_directdcl(string identifier){
    auto direct_dcl = make_shared<DirectDclAST>();
    direct_dcl->name += ": " + identifier;
    direct_dcl->Type = "identifier";
    direct_dcl->id_name = identifier;
    direct_dcl->size = 1;
    return direct_dcl;
}

tnode
make_directdcl_array(tnode direct_dcl, int arrlen){
    
    direct_dcl->name += "is a array size " + to_string(arrlen);
    direct_dcl->Type = "array";
    shared_ptr<DirectDclAST> direct_dcl_t = dynamic_pointer_cast<DirectDclAST>(direct_dcl); 
    direct_dcl_t->size *= arrlen;
    return direct_dcl;
}


tnode
make_parameter_decl(string type_spec, tnode directdcl){
    auto param_decl = make_shared<ParameterDeclAST>();
    param_decl->Type = type_spec;
    param_decl->size = dynamic_pointer_cast<DirectDclAST>(directdcl)->size;
    param_decl->name += " type " + type_spec;
    param_decl->children.push_back(directdcl);
    return param_decl;
}

tnode
make_parameter_list(string type_specifier, tnode directdcl){
    tnode param_list = make_shared<ParameterListAST>();
    auto param_decl = make_parameter_decl(type_specifier, directdcl);
    param_list->children.push_back(param_decl); 
    return param_list;
}

tnode
cons_parameter_list(tnode cdr, string car_type, tnode car_dcl){
    cdr->children.push_back(make_parameter_decl(car_type, car_dcl));
    return cdr;
}

tnode
make_initdcl_list(tnode init_declarator){
    auto i_list = make_shared<InitDeclListAST>();
    i_list->children.push_back(init_declarator);
    return i_list;
}

tnode
cons_initdcl_list(tnode cdr, tnode car){
    cdr->children.push_back(car);
    return cdr;
}

tnode
make_initdcl(tnode direct_dcl, tnode init){
    auto init_dcl = make_shared<InitDeclAST>();
    init_dcl->children.push_back(direct_dcl);
    if(init != nullptr)
        init_dcl->children.push_back(init);
    return init_dcl;
}

tnode
make_init(tnode expr){
    return expr;
}

tnode
make_init_list(tnode init){
    auto init_ls = make_shared<InitListAST>();
    init_ls->children.push_back(init);
    return init_ls;
}

tnode
cons_init_list(tnode cdr, tnode car){
    cdr->children.push_back(car);
    return cdr;
}

tnode
make_loop(tnode cond, tnode body){
    auto loop = make_shared<LoopStatAST>();
    loop->children.push_back(cond);
    loop->children.push_back(body);
    return loop;
}

tnode
make_if(tnode cond, tnode body){
    auto ifstat = make_shared<IfStatAST>();
    ifstat->children.push_back(cond);
    ifstat->children.push_back(body);
    return ifstat;
}

tnode
make_ifelse(tnode cond, tnode body, tnode elbody){
    auto ifelse = make_shared<IfElseStatAST>();
    ifelse->children.push_back(cond);
    ifelse->children.push_back(body);
    ifelse->children.push_back(elbody);
    return ifelse;
}

tnode
make_expr_stat(tnode expr){
    auto exprStat = make_shared<ExprStatAST>();
    exprStat->children.push_back(expr);
    return exprStat;
}

tnode
make_ret_stat(tnode expr){
    auto ret_s = make_shared<RetStatAST>();
    if(expr != nullptr)
        ret_s->children.push_back(expr);
    return ret_s;
}

tnode
make_cont(){
    auto cont = make_shared<ContStatAST>();
    return cont;
}

tnode
make_break(){
    auto brk = make_shared<BreakStatAST>();
    return brk;
}


tnode
make_stat_list(tnode stat){
    auto s_list = make_shared<StatListAST>();
    if(stat != nullptr)
        s_list->children.push_back(stat);
    return s_list;
}

tnode
cons_stat_list(tnode cdr, tnode car){
    cdr->children.push_back(car);
    return cdr;
}

tnode
make_int_c(int i){
    auto constAST = make_shared<ConstAST>();
    constAST->name += " int " + to_string(i);
    constAST->Type = "int";
    constAST->int_v = i;
    return constAST;
}

tnode
make_float_c(double f){
    auto constAST = make_shared<ConstAST>();
    constAST->name += " float " + to_string (f);
    constAST->double_v = f;
    constAST->Type = "float";
    return constAST;
}

tnode
make_char_c(char c){
    auto constAST = make_shared<ConstAST>();
    constAST->name += " char " + to_string(c);
    constAST->Type = "char";
    return constAST;
}

tnode
read_ident(string id){
    auto read_id = make_shared<ReadAST>();
    read_id->name += id;
    return read_id;
}

tnode
func_eval(tnode funcbody, tnode params){
    auto callexpr = make_shared<CallExprAST>();
    callexpr->children.push_back(funcbody);
    callexpr->children.push_back(params);
    return callexpr;
}

tnode
array_get(tnode id, tnode idx){
    auto read_id = make_shared<ReadAST>();
    read_id->children.push_back(id);
    read_id->children.push_back(idx);
    return read_id;    
}

tnode
make_unary(string op, tnode operand){
    auto unexp = make_shared<UnaryExprAST>();
    unexp->name += " operator " + op;
    unexp->children.push_back(operand);
    return unexp;
}

tnode
make_binary(string op, tnode operand1, tnode operand2){
    auto biexp = make_shared<BinaryExprAST>();
    biexp->name += " operator " + op;
    biexp->children.push_back(operand1);
    biexp->children.push_back(operand2);
    return biexp;
}

tnode
make_expr_list(tnode expr){
    auto expls = make_shared<ExprListAST>();
    expls->children.push_back(expr);
    return expls;
}

tnode
cons_expr_list(tnode cdr, tnode car){
    cdr->children.push_back(car);
    return cdr;
}
