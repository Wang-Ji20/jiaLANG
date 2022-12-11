# Language Specification


## 词法规则
    	共有如下六种记号：
                1. 注释。注释由 /* 开始，以 */ 结束，在编译过程中会被忽略。
                2. 标识符。标识符由数字和字母和_组成（首位字母或_），代表一个变量名或者函数名，区分大小写。
                3. 关键字。在假语言中，如下的关键字具有特殊意义，不作为标识符解析。
                        if else while break continue return
                        const int float char void sizeof
                4. 常量。支持整形常量、浮点常量、字符常量、字符串常量。
                5. 运算符。
                6. 空格、换行、分号等特殊符号。

## 类型
        采用静态类型系统。
        int 类型：32位有符号整数
        float 类型：单精度浮点数
        char 类型：8位有符号整数（字符）

## 表达式
        表达式的求值顺序为：
                1. 单一表达式
                        标识符
                        常量
                        (表达式)
                   常量
                        整型常量
                        浮点常量
                        字符常量
                        字符串常量
                2. 后缀表达式
                        单一表达式
                        单一表达式(表达式表)
                        单一表达式[表达式]
                3. 一元表达式
                        后缀表达式
                        (类型名)一元表达式
                        sizeof 一元表达式
    					sizeof(类型名)
                        +一元表达式
                        -一元表达式
                        !一元表达式
                        *一元表达式
                        &一元表达式
                4. 乘法类表达式
                        一元表达式
                        乘法类表达式 * 一元表达式
                        乘法类表达式 / 一元表达式
                        乘法类表达式 % 一元表达式
                5. 加法类表达式
                        乘法类表达式
                        加法类表达式 + 乘法类表达式
                        加法类表达式 - 乘法类表达式
                6. 关系表达式
                        加法类表达式
                        关系表达式 > 加法类表达式
                        关系表达式 < 加法类表达式
                        关系表达式 >= 加法类表达式
                        关系表达式 <= 加法类表达式
                7. 相等类表达式
                        关系表达式
                        相等类表达式 == 关系表达式
                        相等类表达式 != 关系表达式
                8. 逻辑与表达式
                        相等类表达式
                        逻辑与表达式 && 相等类表达式
                9. 逻辑或表达式
                        逻辑与表达式
                        逻辑或表达式 || 逻辑与表达式
                10. 表达式
                        逻辑或表达式
                        一元表达式 = 表达式
                    表达式表
                    	表达式表,opt 表达式

## 语句
        语句
        	表达式语句
        	复合语句
        	选择语句
        	循环语句
        	跳转语句
        表达式语句
            表达式opt ;
        复合语句
            {声明表opt 语句表opt}
        声明表
        	声明表opt 声明
        语句表
        	语句表opt 语句
        选择语句
            if(表达式) 语句
            if(表达式) 语句 else 语句
        循环语句
            while(表达式) 语句
        跳转语句
       		continue;
        	break;
            return 表达式opt;

## 声明
        声明
        	声明说明符 初始化声明符表opt;
        声明说明符
            const opt 类型说明符
        类型说明符
        	void
        	char
        	int
        	float
        初始化声明符表
        	初始化声明符表,opt 初始化声明符
        初始化声明符
        	声明符
        	声明符 = 初值
        声明符
        	指针opt 直接声明符
        指针
        	* const opt 指针opt
        直接声明符
        	标识符
        	(声明符)
        	直接声明符[表达式opt]
        	直接声明符(形式参数表opt)	
    	形式参数表
    		形式参数表,opt 形式参数声明
    	形式参数声明
    		声明说明符 声明符
    		类型名
    	初值
    		表达式
    		{初值表}
    		{初值表,}
    	初值表
    		初值表,opt 初值
    	类型名
    		声明说明符 抽象声明符opt
    	抽象说明符
    		指针
    		指针opt 直接抽象声明符
    	直接抽象声明符
    		(抽象声明符)
    		直接抽象声明符opt [表达式]
    		直接抽象声明符opt (形式参数表opt)

## 函数定义

```
    函数定义：
        声明说明符 声明符 复合语句
    且声明符中必须包含 直接声明符(形式参数表opt)
```

## 作用域

        函数形式参数的运算符在全函数内有效。
        其他标识符在定义的时候开始有效，在程序块结束的时候失效。

## 相比C语言标准的简化

```
仅保留void、char、int、float四种类型。
删除了存储类说明符（auto、register、static、extern、typedef）。
删除了volatile类型限定符。
删除了逗号运算符、条件运算符、位运算符、++运算符、--运算符，赋值运算符仅保留=。
选择语句仅保留if、if else。
循环语句仅保留while。
删除了goto跳转语句。
不支持旧式函数定义。
不支持预处理。
```