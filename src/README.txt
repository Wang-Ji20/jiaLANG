=== The Jia Programming Language  ==

============
= 使用说明 =
============
    输入 make 编译编译器。
    测试程序在 test.jia
    输入 make run 编译运行测试程序。
        由于我们的编译器没有链接器，因此需要 chelper.cc 这个文件辅助。
    
=============
= 源代码说明 =
=============
    AST.cc
        抽象语法树的代码生成部分。这部分为虚函数重载。
    CodegenVisitor.cc  
        用来生成代码的访问者，它会遍历整个抽象语法树。
    Makefile
        构建系统。
    README.txt  
        这个文件。
    driver.cc  
        驱动类，用来把全部东西串联在一起。
    main.cc     
        主程序，处理参数。
    scanner.ll  
        词法分析机。
    AST.hh  
        抽象语法树的定义文件。
    CodegenVisitor.hh  
        生成代码访问者的声明定义文件。
    PrintVisitor.cc  
        debug 用的打印节点访问者。
    chelper.cc  
        用来作为目标代码葡萄架子的辅助文件。
    driver.hh
        驱动类的声明定义文件。
    parser.yy
        语法分析机
    test.jia
        jia语言代码