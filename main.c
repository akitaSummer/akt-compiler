#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#define int int64_t

int MAX_SIZE; // 最大内存

int *code,      // code segment
    *code_dump, // for dump
    *stack;     // stack segment
char *data;     // data segment

int *pc; // 代码指针
int *sp; // stack pointer 栈顶
int *bp; // base pointer 上一个栈的栈顶

int ax, // 寄存器
    cycle;

// symbol table & pointer
int *symbol_table,
    *symbol_ptr,
    *main_ptr;

// VM指令集
enum
{
    // save and load: 寄存器与内存之间的沟通
    IMM,  // 立即数加载指令 加载到内存器
    LEA,  // 寻址指令
    LC,   // 字符加载指令
    LI,   // 整数加载指令
    SC,   // 字符保存指令
    SI,   // 整数保存指令
    PUSH, // 压栈指令

    // a/b/l 运算:各类运算
    ADD, // 加法指令
    SUB, // 减法指令
    MUL, // 乘法指令
    DIV, // 除法指令
    MOD, // 取余指令
    OR,  // 或运算指令
    XOR, // 异或运算指令
    AND, // 与运算指令
    SHL, // 左移运算指令
    SHR, // 右移运算指令
    IQ,  // 相等判断指令
    NE,  // 不等判断指令
    LT,  // 小于判断指令
    GT,  // 大于判断指令
    GE,  // 大于等于判断指令
    EQ,  // 等于判断指令
    LE,  // 小于等于判断指令

    // 分支跳转
    JMP,  // 无条件跳转指令
    JZ,   // 判断为0时跳转指令
    JNZ,  // 判断不为0时跳转指令
    CALL, // 调用指令
    NVAR, // 设置新变量指令
    DARG, // 删除参数指令
    RET,  // 返回指令

    // native-call
    OPEN, // 打开文件指令
    CLOS, // 关闭文件指令
    READ, // 读取文件指令
    PRTF, // 打印格式化指令
    MALC, // 分配内存指令
    FREE, // 释放内存指令
    MEST, // 内存复制指令
    MCMP, // 内存比较指令
    EXIT, // 退出指令
    MSET, // 内存设置指令
};

// symbol_table的key
enum
{
    Token,
    Hash,
    Name,
    Class,
    Type,
    Value,
    GClass,
    GType,
    GValue,
    SymSize
};

// vm
int run_vm(int argc, char **argv)
{
    int op; // 操作符
    int *tmp;
    // 初始化bp, sp位置
    bp = sp = (int *)((int)stack + MAX_SIZE);
    // 初始命令
    *--sp = EXIT;
    *--sp = PUSH;
    tmp = sp;
    *--sp = argc;
    *--sp = (int)argv;
    *--sp = (int)tmp;
    if (!(pc = (int *)main_ptr[Value]))
    {
        printf("main function is not defined\n");
        exit(-1);
    }
    cycle = 0;
    while (1)
    {
        cycle++;
        op = *pc++; // 不断读取指令
        // save and load
        if (op == IMM)
            ax = *pc++; // 读取值到寄存器
        else if (op == LEA)
            ax = (int)(bp + *pc++); // 寻址
        else if (op == LC)
            ax = *(char *)ax; // 将寄存器的值转化为char
        else if (op == LI)
            ax = *(int *)ax; // 将寄存器的值转化为int
        else if (op == SC)
            *(char *)*sp++ = ax; // 将寄存器中的char存储到栈：找到栈顶地址sp->转换成char指针->解引用拿到空间->存储ax的值->sp++退栈
        else if (op == SI)
            *(int *)*sp++ = ax; // 将寄存器中的int存储到栈
        else if (op == PUSH)
            *--sp = ax; // 栈进一步，将寄存器中的值存储到栈
        // 跳转
        else if (op == JMP)
            pc = (int *)*pc; // 跳转至目标地址
        else if (op == JZ)
            pc = ax ? pc + 1 : (int *)*pc; // if ax == 0，执行下一行，否则跳转至目标地址
        else if (op == JNZ)
            pc = ax ? (int *)*pc : pc + 1; //  if ax != 0，执行下一行，否则跳转至目标地址
        // 运算
        else if (op == OR)
            ax = *sp++ | ax;
        else if (op == XOR)
            ax = *sp++ ^ ax;
        else if (op == AND)
            ax = *sp++ & ax;
        else if (op == EQ)
            ax = *sp++ == ax;
        else if (op == NE)
            ax = *sp++ != ax;
        else if (op == LT)
            ax = *sp++ < ax;
        else if (op == LE)
            ax = *sp++ <= ax;
        else if (op == GT)
            ax = *sp++ > ax;
        else if (op == GE)
            ax = *sp++ >= ax;
        else if (op == SHL)
            ax = *sp++ << ax;
        else if (op == SHR)
            ax = *sp++ >> ax;
        else if (op == ADD)
            ax = *sp++ + ax;
        else if (op == SUB)
            ax = *sp++ - ax;
        else if (op == MUL)
            ax = *sp++ * ax;
        else if (op == DIV)
            ax = *sp++ / ax;
        else if (op == MOD)
            ax = *sp++ % ax;
        // 将CALL下一位，也就是函数调用结束后返回的地址压入栈，并且跳转至函数地址
        else if (op == CALL)
        {
            *--sp = (int)(pc + 1);
            pc = (int *)*pc;
        }
        // 进入函数后，会创建新的栈空间，先将上一个栈的栈顶进行储存至bp中，然后为栈创建初始空间
        else if (op == NVAR)
        {
            *--sp = (int)bp;
            bp = sp;
            sp = sp - *pc++;
        }
        // 销毁栈的空间，把参数赋值都删掉
        else if (op == DARG)
            sp = sp + *pc++;
        // 执行return: 将bp还原给sp，然后将还原后的sp的bp位置，赋值给bp，sp再退一步，将pc还原，sp再退一步到DARG位置
        else if (op == RET)
        {
            sp = bp;
            bp = (int *)*sp++;
            pc = (int *)*sp++;
        }
        // native call
        else if (op == OPEN)
        {
            ax = open((char *)sp[1], sp[0]);
        }
        else if (op == CLOS)
        {
            ax = close(*sp);
        }
        else if (op == READ)
        {
            ax = read(sp[2], (char *)sp[1], *sp);
        }
        else if (op == PRTF)
        {
            tmp = sp + pc[1] - 1;
            ax = printf((char *)tmp[0], tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5]);
        }
        else if (op == MALC)
        {
            ax = (int)malloc(*sp);
        }
        else if (op == FREE)
        {
            free((void *)*sp);
        }
        else if (op == MSET)
        {
            ax = (int)memset((char *)sp[2], sp[1], *sp);
        }
        else if (op == MCMP)
        {
            ax = memcmp((char *)sp[2], (char *)sp[1], *sp);
        }
        else if (op == EXIT)
        {
            printf("exit(%lld)\n", *sp);
            return *sp;
        }
        else
        {
            printf("unkown instruction: %lld, cycle: %lld\n", op, cycle);
            return -1;
        }
    }
    return 0;
}