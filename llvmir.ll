; ModuleID = 'test'
; 分号是注释
source_filename = "test"
; 这里的 module 指的是源文件

; 定义了一个函数 返回值 void 名字为 ass
; 两个参数 i64 => int 64bit == c语言的 uint64_t 

define void @ass(i64 %a, i64 %b) {
; xxxx: 这个叫“块” block 是一段代码
entry:
  ; 下面这句话相当于 汇编语言的:
  ; sub $0x8, %rsp
  %0 = alloca i64, align 8
  store i64 %a, i64* %0, align 4
  %1 = alloca i64, align 8
  store i64 %b, i64* %1, align 4
  %2 = alloca i64, align 8
  ret void
  ; 这段函数的作用是
  ; 什么也不做
}

; 这个函数的功能是，拿一个变量，取地址，然后取地址的内容，然后返回这个内容
define i64 @addr(i64 %a) {
entry:
  %0 = alloca i64, align 8
  store i64 %a, i64* %0, align 4
  ; *(%0) = %a;
  %1 = load i64, i64* %0, align 4
  ; int a = *(%0);
  %2 = ptrtoint i64* %0 to i64
  ; int64 b = (int64)%0;
  %3 = inttoptr i64 %2 to i64*
  ; int* c = (int*)b; 
  %4 = load i64, i64* %3, align 4
  ; int d = *c;
  ret i64 %4
  ; return d;
  ; 我相信是这样 这台抽象了
  ; 这背后有深刻的原因
  ; 首先，llvm 有问题 llvm 的 每个变量只能赋值一次 都是常量 所以每次值的改变都会产生一个新的变量 行
}

; 这个函数的功能是整数相加
define i64 @iadd(i64 %a, i64 %b) {
entry:
  %0 = alloca i64, align 8
  store i64 %a, i64* %0, align 4
  %1 = alloca i64, align 8
  store i64 %b, i64* %1, align 4
  ; 上面这四步是参数传递
  
  %2 = alloca i64, align 8
  ; 我不知为什么会有这步 很迷

  %3 = load i64, i64* %0, align 4
  %4 = load i64, i64* %1, align 4
  %5 = add i64 %3, %4 
  ret i64 %5
}

; 浮点数相加，没啥意思 跳过
define double @ffadd(double %a, double %b) {
entry:
  %0 = alloca double, align 8
  store double %a, double* %0, align 8
  %1 = alloca double, align 8
  store double %b, double* %1, align 8
  %2 = load double, double* %0, align 8
  %3 = load double, double* %1, align 8
  %4 = fadd double %2, %3
  ret double %4
}

; 一些复杂的算术运算 也没啥意思 跳过
define i64 @complex_comp(i64 %a, i64 %b, i64 %c, i64 %d) {
entry:
  %0 = alloca i64, align 8
  store i64 %a, i64* %0, align 4
  %1 = alloca i64, align 8
  store i64 %b, i64* %1, align 4
  %2 = alloca i64, align 8
  store i64 %c, i64* %2, align 4
  %3 = alloca i64, align 8
  store i64 %d, i64* %3, align 4
  %4 = load i64, i64* %0, align 4
  %5 = load i64, i64* %1, align 4
  %6 = add i64 %4, %5
  %7 = load i64, i64* %2, align 4
  %8 = mul i64 %6, %7
  %9 = load i64, i64* %3, align 4
  %10 = srem i64 %8, %9
  ret i64 %10
}

; 这个，我创建了一个局部变量，返回了局部变量的值
define i64 @always10() {
entry:
  %0 = alloca i64, align 8
  %1 = load i64, i64* %0, align 4
  ; 局部变量等于 10
  store i64 10, i64* %0, align 4
  %2 = load i64, i64* %0, align 4
  ret i64 %2
}

; 绝对值函数
define i64 @myabs(i64 %a) {
entry:
  %0 = alloca i64, align 8
  store i64 %a, i64* %0, align 4
  %1 = alloca i64, align 8
  %2 = load i64, i64* %0, align 4
  ; 你应该可以看出 %2 = a correct!

  ; 这个的意思是 比较是否 a < 0 slt => s 带符号 lt < less than ok
  %3 = icmp slt i64 %2, 0
  ; br == branch 根据 %3 判断跳到 %4 还是 %8
  br i1 %3, label %4, label %8

4:                                                ; preds = %entry
  ; 这应该是 取了个相反数
  %5 = load i64, i64* %1, align 4
  %6 = load i64, i64* %0, align 4
  %7 = sub i64 0, %6
  store i64 %7, i64* %1, align 4
  br label %11

8:
; 正数不变                                                ; preds = %entry
  %9 = load i64, i64* %1, align 4
  %10 = load i64, i64* %0, align 4
  store i64 %10, i64* %1, align 4
  br label %11

11:                                               ; preds = %8, %4
  %12 = load i64, i64* %1, align 4
  ; 返回最终的值
  ret i64 %12
}

define i64 @mygcd(i64 %a, i64 %b) {
entry:
  %0 = alloca i64, align 8
  store i64 %a, i64* %0, align 4
  %1 = alloca i64, align 8
  store i64 %b, i64* %1, align 4
  %2 = alloca i64, align 8 
  %3 = load i64, i64* %2, align 4
  %4 = load i64, i64* %0, align 4
  %5 = load i64, i64* %1, align 4
  %6 = srem i64 %4, %5
  store i64 %6, i64* %2, align 4
  %7 = load i64, i64* %2, align 4
  %8 = icmp ne i64 %7, 0
  br i1 %8, label %9, label %20

9:                                                ; preds = %9, %entry
  %10 = load i64, i64* %0, align 4
  %11 = load i64, i64* %1, align 4
  store i64 %11, i64* %0, align 4
  %12 = load i64, i64* %1, align 4
  %13 = load i64, i64* %2, align 4
  store i64 %13, i64* %1, align 4
  %14 = load i64, i64* %2, align 4
  %15 = load i64, i64* %0, align 4
  %16 = load i64, i64* %1, align 4
  %17 = srem i64 %15, %16
  store i64 %17, i64* %2, align 4
  %18 = load i64, i64* %2, align 4
  %19 = icmp ne i64 %18, 0
  br i1 %19, label %9, label %20

20:                                               ; preds = %9, %entry
  %21 = load i64, i64* %1, align 4
  ret i64 %21
}