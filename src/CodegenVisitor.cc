#include "CodegenVisitor.hh"
#include "AST.hh"

using namespace llvm;
using namespace std;

// 有点长， Don't Panic!

void panic(std::string s){
    fprintf(stderr, "%s", s.c_str());
    exit(-1);
}


// visit 是入口方法
void CodegenVisitor::visit(std::shared_ptr<AST> ast){
    BasicBlock *BB = BasicBlock::Create(*TheContext, "ee");
    Builder->SetInsertPoint(BB);
    for (auto &&i : ast->children)
    {
        i->codegen(*this); // 从根节点出发向下生成代码
    }
    return;
}


// 生成常数值 
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

// 这个是新建声明
Value*
DeclAST::codegen(CodegenVisitor& v){

    /* get type, direct_dcl and initializer */
    llvm::Type* vtype;
    Value* vvalue;
    Constant* default_init;

    // parse 的时候把 Type 解析成字符串，现在需要弄成 llvm 类型
    // 初始化值为 0
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
    /* int a, b, c, d; 这个 for 扫 a b c d 这个列表 */
    for (auto &&init_dcl : children[0]->children)
    {
        printf("auto type name %s\n", init_dcl->name.c_str());
        
        // direct_dcl
        // 初始化声明符有 孩子： 直接声明符 & （初始化 可选）
        if(init_dcl->children.size() >= 1){
            // 直接声明符
            std::shared_ptr<DirectDclAST> decl = std::dynamic_pointer_cast<DirectDclAST>(init_dcl->children[0]);
            printf("meet direct dcl %s with size %d\n", decl->id_name.c_str(), decl->size);

            // ！直接声明符有大小 大小为1是标量 大于 1 是 数组
            if (decl->size == 1)
            {
                printf("create a new variable\n");
                // global?
                // 如果符号表只有一个，就是全局变量，用 module 创建
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
                // 如果是局部变量 就直接 Create alloca 你记不记得 alloca 1 就是 ir 的 alloca 语句
                else
                {
                    vvalue = v.Builder->CreateAlloca(vtype); 
                }

                printf("alloc\n");
                // 在符号表中插入
                v.SymbolTables.back()[decl->id_name] = vvalue;
                printf("map\n");
                
                // direct_dcl has initialzer
                // 局部变量初始化 我还没有做
                if (decl->size == 2)
                {
                    panic("not implemented - local variable init");
                }
                
                printf("created a new variable\n");
            }
            // 数组 内容极其少 其实应该主要是初始化比较难 hao
            else{
                Value* arraySize = ConstantInt::get(Type::getInt64Ty(*v.TheContext), decl->size, true);
                auto arrayType = ArrayType::get(vtype, decl->size);
                vvalue = v.Builder->CreateAlloca(arrayType, arraySize, "");
                v.SymbolTables.back()[decl->id_name] = vvalue;
            }
        }
        else{
            panic("wrong init_dcl format\n");
        }
    }
    
    return vvalue;
}

// 函数定义！
Value*
FuncDefAST::codegen(CodegenVisitor& v){
    printf("start function codegen.\n");

    if(children.size() < 2 || children.size() > 3)
        panic("funcDefAST: bad children num\n");

    // 函数的 直接声明符
    auto direct_dcl = dynamic_pointer_cast<DirectDclAST>(children[0]);
    // 函数参数
    tnode params = nullptr;
    // 函数体
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

    v.Builder->SetInsertPoint(BBEntry);//? 你记不记得 ir 里 见过 entry: dui 
    // 就是 上面这两行的功能
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
                allocv = v.Builder->CreateAlloca(PointerType::get(v.s2Type((*arglist)->Type), 0)); // 这里就是数组传指针了
            }

            v.Builder->CreateStore(&func_args, allocv, false); // 前面的 store 都是这里来的 wo

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

// 一堆语句 这是 {int a; .... ;} 的结点
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

// 返回值
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

// 一元运算符
llvm::Value*
UnaryExprAST::codegen(CodegenVisitor& v){
    // 求操作数的值
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
        // ... 所以 int 也要转回指针 hao
        auto vderef = v.Builder->CreateIntToPtr(operand, Type::getInt64PtrTy(*v.TheContext));
        return v.Builder->CreateLoad(v.s2Type("int"), vderef, "");
    }
    else if (name == "&")
    {
        auto oaddr = v.getSymbolValue(children[0]->name);
        // 转 int 是因为我们没有指针类
        return v.Builder->CreatePtrToInt(oaddr, v.s2Type("int"));
    }
    return nullptr;
    
}

// 二元运算符 很无聊的
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

       // deal with pointer?
        if (oaddr->getType()->getPointerElementType()->isPointerTy())
        {
            oaddr = v.Builder->CreateGEP(oaddr->getType()->getPointerElementType()->getPointerElementType(),
                v.Builder->CreateLoad(oaddr->getType()->getPointerElementType(), oaddr),
                children[0]->children[1]->codegen(v)
            );
        }
        

       return v.Builder->CreateStore(rop, oaddr);
    }
    
    
}

// 读取标识符的数值
llvm::Value*
ReadAST::codegen(CodegenVisitor& v){
    printf("read value\n");
    v.outSymbolTable();
    Value* id_ptr_value = v.getSymbolValue(name);

    if (id_ptr_value == nullptr)
    {
        panic("undefined variable.\n");
    }

    Value* id_value = v.Builder->CreateLoad(id_ptr_value->getType()->getPointerElementType(), id_ptr_value, ""); // 刚刚看到的 load 是这里来的 hao

    if (id_value == nullptr)
    {
        panic("undefined variable.\n");
    }
    if(id_value->getType()->isPointerTy()){
        Value* array_value = v.Builder->CreateGEP(id_value->getType()->getPointerElementType(), id_value,
        children[1]->codegen(v), ""
        );
        return v.Builder->CreateLoad(id_value->getType()->getPointerElementType(), array_value);
    }
    
    return id_value;
}

// 表达式语句 很平凡
Value*
ExprStatAST::codegen(CodegenVisitor& v){
    return children[0]->codegen(v);
}

// if else
Value*
IfElseStatAST::codegen(CodegenVisitor& v){
    printf("ifelse stat\n");

    Value* condition = children[0]->codegen(v);

    if(condition == nullptr)
        panic("condition fault\n");

    Function* functx = v.Builder->GetInsertBlock()->getParent(); // 获取 现在 在哪个函数里

    BasicBlock* thenBB = BasicBlock::Create(*v.TheContext, "", functx);
    BasicBlock* elseBB = BasicBlock::Create(*v.TheContext, "");
    BasicBlock* outBB = BasicBlock::Create(*v.TheContext, ""); // if else 外面

    v.Builder->CreateCondBr(condition, thenBB, elseBB);

    v.Builder->SetInsertPoint(thenBB); // 在这个标签块里面插入

    map<string, Value*> st;
    v.SymbolTables.push_back(st);

    children[1]->codegen(v);

    v.SymbolTables.pop_back();

    if (thenBB->getTerminator() == nullptr) // 如果 thenBB 里没有 return 那还要回到 if 外面 函数里面的地方
    {
        v.Builder->CreateBr(outBB);
    }

    functx->getBasicBlockList().push_back(elseBB); // 在函数里加入块

    v.Builder->SetInsertPoint(elseBB);

    v.SymbolTables.push_back(st);

    children[2]->codegen(v);

    v.SymbolTables.pop_back();

    v.Builder->CreateBr(outBB);
    
    
    functx->getBasicBlockList().push_back(outBB);

    v.Builder->SetInsertPoint(outBB);

    return nullptr;

}

llvm::Value*
LoopStatAST::codegen(CodegenVisitor& v){
    Function* functx = v.Builder->GetInsertBlock()->getParent();

    BasicBlock* loopbody = BasicBlock::Create(*v.TheContext, "", functx);
    BasicBlock* outbody = BasicBlock::Create(*v.TheContext, "");

    Value* condition = children[0]->codegen(v);

    v.Builder->CreateCondBr(condition, loopbody, outbody);

    v.Builder->SetInsertPoint(loopbody);
    
    map<string, Value*> st;
    v.SymbolTables.push_back(st);
    v.loopHeads.push_back(loopbody);
    v.loopEnds.push_back(outbody);

    children[1]->codegen(v);

    v.SymbolTables.pop_back();
    v.loopHeads.pop_back();
    v.loopEnds.pop_back();

    condition = children[0]->codegen(v);
    v.Builder->CreateCondBr(condition, loopbody, outbody);
    

    functx->getBasicBlockList().push_back(outbody);

    v.Builder->SetInsertPoint(outbody);



    return nullptr;

}

Value*
ContStatAST::codegen(CodegenVisitor& v)
{
    return v.Builder->CreateBr(v.loopHeads.back());
}

Value*
BreakStatAST::codegen(CodegenVisitor& v)
{
    return v.Builder->CreateBr(v.loopEnds.back());
}

Value*
CallExprAST::codegen(CodegenVisitor& v)
{
    printf("call expression.\n");
    Function* func = v.TheModule->getFunction(children[0]->name);

    if(func == nullptr){
        panic("called invalid function.\n");
    }

    if (func->arg_size() != children[1]->children.size())
    {
        panic("called args invalid number.\n");
    }

    vector<Value*> argsv;

    for (auto &&callerarg : children[1]->children)
    {
        argsv.push_back(callerarg->codegen(v));
    }

    return v.Builder->CreateCall(func, argsv, "");
}

