#include <cstdio>
#include <cstdlib>
#include <ucontext.h>

/*
// 上下文结构体定义
// 这个结构体是平台相关的，因为不同平台的寄存器不一样
// 下面列出的是所有平台都至少会包含的4个成员
typedef struct ucontext_t {
    // 当前上下文结束后，下一个激活的上下文对象的指针，只在当前上下文是由makecontext创建时有效
    struct ucontext_t *uc_link;
    // 当前上下文的信号屏蔽掩码
    sigset_t          uc_sigmask;
    // 当前上下文使用的栈内存空间，只在当前上下文是由makecontext创建时有效
    stack_t           uc_stack;
    // 平台相关的上下文具体内容，包含寄存器的值
    mcontext_t        uc_mcontext;
    ...
} ucontext_t;
 
// 获取当前的上下文
int getcontext(ucontext_t *ucp);
 
// 恢复ucp指向的上下文，这个函数不会返回，而是会跳转到ucp上下文对应的函数中执行，相当于变相调用了函数
int setcontext(const ucontext_t *ucp);
 
// 修改由getcontext获取到的上下文指针ucp，将其与一个函数func进行绑定，支持指定func运行时的参数，
// 在调用makecontext之前，必须手动给ucp分配一段内存空间，存储在ucp->uc_stack中，这段内存空间将作为func函数运行时的栈空间，
// 同时也可以指定ucp->uc_link，表示函数运行结束后恢复uc_link指向的上下文，
// 如果不赋值uc_link，那func函数结束时必须调用setcontext或swapcontext以重新指定一个有效的上下文，否则程序就跑飞了
// makecontext执行完后，ucp就与函数func绑定了，调用setcontext或swapcontext激活ucp时，func就会被运行
void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...);
 
// 恢复ucp指向的上下文，同时将当前的上下文存储到oucp中，
// 和setcontext一样，swapcontext也不会返回，而是会跳转到ucp上下文对应的函数中执行，相当于调用了函数
// swapcontext是sylar非对称协程实现的关键，线程主协程和子协程用这个接口进行上下文切换
int swapcontext(ucontext_t *oucp, const ucontext_t *ucp);
*/

static ucontext_t s_uctx_main, s_uctx_fun1, s_uctx_fun2;

#define handle_error(msg)   \
    do {                    \
        perror(msg);        \
        exit(EXIT_FAILURE);  \
    }while(0)               \

static void fun1() {
    printf("fun1: started\n");
    printf("fun1: swapcontext from fun1 to fun2\n");
    // 激活fun2上下文
    if(-1 == swapcontext(&s_uctx_fun1, &s_uctx_fun2)) {
        handle_error("swapcontext from fun1 to fun2");
    }
    // fun1结束
    printf("fun1: return\n");
}

static void fun2() {
    printf("fun2: started\n");
    printf("fun2: swapcontext from fun2 to fun1\n");
    // 激活fun1上下文
    if(-1 == swapcontext(&s_uctx_fun2, &s_uctx_fun1)) {
        handle_error("swapcontext from fun2 to fun1");
    }
    printf("fun2: return\n");
}

int main(int argc, char* argv[]) {
    char fun1_stack[16384];
    char fun2_stack[16384];

    // 获取fun1上下文
    if(-1 == getcontext(&s_uctx_fun1)) {
        handle_error("getcontext fun1");
    }

    // 初始化fun1上下文
    s_uctx_fun1.uc_link = &s_uctx_main;
    s_uctx_fun1.uc_stack.ss_sp = fun1_stack;
    s_uctx_fun1.uc_stack.ss_size = sizeof(fun1_stack);
    makecontext(&s_uctx_fun1, fun1, 0);

    // 获取fun2上下文
    if(-1 == getcontext(&s_uctx_fun2)) {
        handle_error("getcontext fun1");
    }

    // 初始化fun2上下文
    s_uctx_fun2.uc_link = (argc > 1) ? NULL : &s_uctx_fun1;
    s_uctx_fun2.uc_stack.ss_sp = fun2_stack;
    s_uctx_fun2.uc_stack.ss_size = sizeof(fun2_stack);
    makecontext(&s_uctx_fun2, fun2, 0);

    // 激活fun2上下文，同时将旧的上下文存储到main中
    printf("swapcontex from main to fun2\n");
    if(-1 == swapcontext(&s_uctx_main, &s_uctx_fun2)) {
        handle_error("swapcontext from main to fun2");
    }

    // 等fun1结束
    printf("main: exit\n");
    return 0;
}