#include "CodegenVisitor.hh"
#include "AST.hh"

using namespace llvm;
using namespace std;

void panic(std::string s){
    fprintf(stderr, "%s", s.c_str());
    exit(0);
}



void CodegenVisitor::visit(std::shared_ptr<AST> ast){
    BasicBlock *BB = BasicBlock::Create(*TheContext, "ee");
    Builder->SetInsertPoint(BB);
    for (auto &&i : ast->children)
    {
        i->codegen(*this);
    }
    return;
}


Value*
ConstAST::codegen(CodegenVisitor& v){
    if (Type == "int")
    {
        return ConstantInt::get(Type::getInt64Ty(*v.TheContext), int_v, true);
    }
    else if (Type == "float")
    {
        return llvm::ConstantFP::get(Type::getDoubleTy(*v.TheContext), double_v);
    }
    else if (Type == "char")
    {
        return ConstantInt::get(Type::getInt8Ty(*v.TheContext), c_v, true);
    }
    panic("constAST error\n");
    return nullptr;
}

Value*
DeclAST::codegen(CodegenVisitor& v){

    /* get type, direct_dcl and initializer */
    llvm::Type* vtype;
    Value* vvalue;
    Constant* default_init;

    if(Type == "int")
    {
        vtype = Type::getInt64Ty(*v.TheContext);
        default_init = ConstantInt::get(Type::getInt64Ty(*v.TheContext), 0, true);
        printf("recognize decl type int\n");
    }
    else if(Type == "float")
    {
        vtype = Type::getDoubleTy(*v.TheContext);
        default_init = llvm::ConstantFP::get(Type::getDoubleTy(*v.TheContext), 0);
    }
    else if(Type == "char")
    {
        vtype = Type::getInt8Ty(*v.TheContext);
        default_init = ConstantInt::get(Type::getInt8Ty(*v.TheContext), 0, true);
    }

    printf("generate decl for type %s\n", Type.c_str());

    /* iterate all inits */
    for (auto &&init_dcl : children[0]->children)
    {
        printf("auto type name %s\n", init_dcl->name.c_str());
        
        // direct_dcl
        if(init_dcl->children.size() >= 1){
            std::shared_ptr<DirectDclAST> decl = std::dynamic_pointer_cast<DirectDclAST>(init_dcl->children[0]);
            printf("meet direct dcl %s with size %d\n", decl->id_name.c_str(), decl->size);
            if (decl->size == 1)
            {
                printf("create a new variable\n");
                // global?
                if(v.SymbolTables.size() == 1)
                {
                    v.TheModule->getOrInsertGlobal(decl->id_name, vtype);
                    auto gv = v.TheModule->getGlobalVariable(decl->id_name);
                    if(init_dcl->children.size() == 2){
                        printf("start init\n");
                        default_init = dyn_cast<Constant>(init_dcl->children[1]->codegen(v));
                        if(default_init == nullptr){
                            panic("bad init value\n");
                        }
                    }
                    gv->setInitializer(default_init);
                    gv->setAlignment(MaybeAlign(8));
                    vvalue = gv;
                }
                // local
                else
                {
                    vvalue = v.Builder->CreateAlloca(vtype); 
                }

                printf("alloc\n");
                v.SymbolTables.back()[decl->id_name] = vvalue;
                printf("map\n");
                
                // direct_dcl has initialzer
                if (decl->size == 2)
                {
                    
                }
                
                printf("created a new variable\n");
            }
            else{
                Value* arraySize = ConstantInt::get(Type::getInt64Ty(*v.TheContext), decl->size, true);
                auto arrayType = ArrayType::get(vtype, decl->size);
                vvalue = v.Builder->CreateAlloca(arrayType, arraySize, "garray");
            }
        }
        else{
            panic("wrong init_dcl format\n");
        }
    }
    
    return vvalue;
}

Value*
FuncDefAST::codegen(CodegenVisitor& v){
    printf("start function codegen.\n");

    if(children.size() < 2 || children.size() > 3)
        panic("funcDefAST: bad children num\n");

    auto direct_dcl = dynamic_pointer_cast<DirectDclAST>(children[0]);
    tnode params = nullptr;
    tnode func_body;
    if(children.size() == 3){
        params = children[1];
        func_body = children[2];
    }
    else
        func_body = children[1];

    printf("- producing func signature\n");
    vector<llvm::Type*> funcargs;

    if(params != nullptr){
        for (auto &&arg : params->children)
        {
            auto pd_arg = dynamic_pointer_cast<ParameterDeclAST>(arg);
            if(pd_arg->size == 1){
                funcargs.push_back(v.s2Type(pd_arg->Type));
            }
            else
            {
                funcargs.push_back(PointerType::get(v.s2Type(pd_arg->Type), 0));
            }
        }
    }
    
    llvm::Type* retType = v.s2Type(Type);

    FunctionType* ft;
    if(params != nullptr)
        ft = FunctionType::get(retType, funcargs, false);
    else
        ft = FunctionType::get(retType, false);

    Function* func = Function::Create(ft,
        GlobalValue::ExternalLinkage, direct_dcl->id_name.c_str(), *v.TheModule.get());

    printf("- producing function body\n");

    BasicBlock* BBEntry = BasicBlock::Create(*v.TheContext, "entry", func, nullptr);

    v.Builder->SetInsertPoint(BBEntry);

    // new symbol table

    map<string, Value*> funcST;

    // define func args
    if(params != nullptr)
    {
        auto arglist = params->children.begin();
        for (auto &&func_args : func->args())
        {
            auto direct_dcl = (dynamic_pointer_cast<DirectDclAST>((*arglist)->children[0]));
            func_args.setName(direct_dcl->id_name);
            Value* allocv;
            if(direct_dcl->size == 1){
                allocv = v.Builder->CreateAlloca(v.s2Type((*arglist)->Type));
            }
            else{
                allocv = v.Builder->CreateAlloca(PointerType::get(v.s2Type((*arglist)->Type), 0));
            }

            v.Builder->CreateStore(&func_args, allocv, false);

            funcST[direct_dcl->id_name] = allocv;

            arglist++;
        }
    }

    v.SymbolTables.push_back(funcST);
    
    func_body->codegen(v);

    // return

    // if(Type == "void")
    //     v.Builder->CreateRetVoid();
    // else
    // {
    //     v.Builder->CreateRet(v.retValues.back());
    //     v.retValues.pop_back();
    // }

    verifyFunction(*func);
    
    v.SymbolTables.pop_back();

    return func;
}

llvm::Value*
StatListAST::codegen(CodegenVisitor& v){
    printf("\ngenerating block.\n");
    Value* vl;
    for (auto &&stat : children)
    {
        vl = stat->codegen(v);
    }
    return vl;
    
}

llvm::Value*
RetStatAST::codegen(CodegenVisitor& v){
    printf("\ngenerating return stat\n");
    if (children.size() == 0)
    {
        v.Builder->CreateRetVoid();
        return nullptr;
    }
    else
    {
        Value* vl = children[0]->codegen(v);
        v.Builder->CreateRet(vl);
        return vl;
    }
    
}

llvm::Value*
UnaryExprAST::codegen(CodegenVisitor& v){
    Value* operand = children[0]->codegen(v);
    if (name == "-")
    {
        if(operand->getType()->isIntegerTy())
            return v.Builder->CreateNeg(operand);
        else
            return v.Builder->CreateFNeg(operand);
    }
    else if (name == "!")
    {
        return v.Builder->CreateNot(operand);
    }
    else if (name == "*")
    {
        auto vderef = v.Builder->CreateIntToPtr(operand, Type::getInt64PtrTy(*v.TheContext));
        return v.Builder->CreateLoad(v.s2Type("int"), vderef, "");
    }
    else if (name == "&")
    {
        auto oaddr = v.getSymbolValue(children[0]->name);
        return v.Builder->CreatePtrToInt(oaddr, v.s2Type("int"));
    }
    return nullptr;
    
}

llvm::Value*
BinaryExprAST::codegen(CodegenVisitor& v){
    Value* lop = children[0]->codegen(v);
    Value* rop = children[1]->codegen(v);
    if(name == "+"){
        if(lop->getType()->isIntegerTy())
            return v.Builder->CreateAdd(lop, rop);
        else
            return v.Builder->CreateFAdd(lop, rop);
    }
    else if (name == "*")
    {
        if(lop->getType()->isIntegerTy())
            return v.Builder->CreateMul(lop, rop);
        else
            return v.Builder->CreateFMul(lop, rop);
    }
    else if (name == "-")
    {
        if(lop->getType()->isIntegerTy())
            return v.Builder->CreateSub(lop, rop);
        else
            return v.Builder->CreateFSub(lop, rop);        
    }
    else if (name == "/")
    {
        if(lop->getType()->isIntegerTy())
            return v.Builder->CreateSDiv(lop, rop);
        else
            return v.Builder->CreateFDiv(lop, rop);        
    }
    else if (name == "%")
    {
        if(lop->getType()->isIntegerTy())
            return v.Builder->CreateSRem(lop, rop);
        else
            return v.Builder->CreateFRem(lop, rop);
    }
    else if (name == ">")
    {
        if(lop->getType()->isIntegerTy())
            return v.Builder->CreateICmpSGT(lop, rop);
        else
            return v.Builder->CreateFCmpOGT(lop, rop);
    }
    else if (name == "<")
    {
        if(lop->getType()->isIntegerTy())
            return v.Builder->CreateICmpSLT(lop, rop);
        else
            return v.Builder->CreateFCmpOLT(lop, rop);
    }
    else if (name == ">=")
    {
        if(lop->getType()->isIntegerTy())
            return v.Builder->CreateICmpSGE(lop, rop);
        else
            return v.Builder->CreateFCmpOGE(lop, rop);
    }
    else if (name == "<=")
    {
        if(lop->getType()->isIntegerTy())
            return v.Builder->CreateICmpSLE(lop, rop);
        else
            return v.Builder->CreateFCmpOLE(lop, rop);
    }
    else if (name == "==")
    {
        if(lop->getType()->isIntegerTy())
            return v.Builder->CreateICmpEQ(lop, rop);
        else
            return v.Builder->CreateFCmpOEQ(lop, rop);
    }
    else if (name == "!=")
    {
        if(lop->getType()->isIntegerTy())
            return v.Builder->CreateICmpNE(lop, rop);
        else
            return v.Builder->CreateFCmpONE(lop, rop);
    }
    else if (name == "&&")
    {
        return v.Builder->CreateLogicalAnd(lop, rop);
    }
    else if (name == "||")
    {
        return v.Builder->CreateLogicalOr(lop, rop);
    }
    else if (name == "=")
    {
       auto oaddr = v.getSymbolValue(children[0]->name);
       if(oaddr == nullptr){
        panic("bad left value\n");
       }
       return v.Builder->CreateStore(rop, oaddr);
    }
    
    
}

llvm::Value*
ReadAST::codegen(CodegenVisitor& v){
    printf("read value\n");
    v.outSymbolTable();
    Value* id_ptr_value = v.getSymbolValue(name);

    if (id_ptr_value == nullptr)
    {
        panic("undefined variable.\n");
    }

    Value* id_value = v.Builder->CreateLoad(id_ptr_value->getType()->getPointerElementType(), id_ptr_value, "");

    if (id_value == nullptr)
    {
        panic("undefined variable.\n");
    }
    if(id_value->getType()->isPointerTy()){
        panic("array not implemented!\n");
    }
    
    return id_value;
}

Value*
ExprStatAST::codegen(CodegenVisitor& v){
    return children[0]->codegen(v);
}