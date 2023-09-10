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

int *symbol_table; // 用于储存信息
int *symbol_ptr;   // 储存信息
int *main_ptr;

int token, token_val;
int line;

// src code & dump
char *src,
    *src_dump;

// VM指令集
enum
{
    IMM,  // 读取值到内存器
    LEA,  // 内存地址
    JMP,  // 无条件跳转
    JZ,   // 判断为零跳转
    JNZ,  // 判断不为零跳转
    CALL, // 调用函数
    NVAR, // 新建变量
    DARG, // 函数参数
    RET,  // 返回指令
    LI,   // 加载整型
    LC,   // 加载字符
    SI,   // 存储整型
    SC,   // 存储字符
    PUSH, // 压栈
    OR,   // 逻辑或
    XOR,  // 逻辑异或
    AND,  // 逻辑与
    EQ,   // 相等比较
    NE,   // 不等比较
    LT,   // 小于比较
    GT,   // 大于比较
    LE,   // 小于等于比较
    GE,   // 大于等于比较
    SHL,  // 左移
    SHR,  // 右移
    ADD,  // 加法
    SUB,  // 减法
    MUL,  // 乘法
    DIV,  // 除法
    MOD,  // 取余
    OPEN, // 打开文件
    READ, // 读取文件
    CLOS, // 关闭文件
    PRTF, // 打印格式化输出
    MALC, // 内存分配
    FREE, // 释放内存
    MSET, // 内存设置
    MCMP, // 内存比较
    EXIT  // 退出程序
};

// 关键词
enum
{
    Num = 128,
    Fun,
    Sys,
    Glo,
    Loc,
    Id,
    Char,
    Int,
    Enum,
    If,
    Else,
    Return,
    Sizeof,
    While,
    // 操作符
    Assign,
    Cond,
    Lor,
    Land,
    Or,
    Xor,
    And,
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,
    Shl,
    Shr,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Inc,
    Dec,
    Brak
};

// symbol_table的key
enum
{
    // 词法分析
    Token, // 关键字，定义的遍历或者函数名
    Hash,  // 名称会压缩成hash，为了以后查找方便
    Name,  // 名称

    // 语法分析
    Class, // num(比如enum)/func/sys/loc局部变量/glo全局变量
    Type,  // char/int/pointer
    Value,
    // G开头的是变量遮蔽，即在函数的局部变量中时，会将全局的同名变量的信息存储至G开头的key中
    GClass,
    GType,
    GValue,
    SymSize
};

enum
{
    CHAR,
    INT,
    PTR
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

// 词法解析
void tokenize()
{
    char *ch_ptr;
    // 不断读取源码的字符
    while ((token = *src++))
    {
        // debug用
        if (token == '\n')
            line++;
        // 不管宏
        else if (token == '#')
            while (*src != 0 && *src != '\n')
                src++;
        // 大小写和下划线开头的是符号
        else if ((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || (token == '_'))
        {
            ch_ptr = src - 1;
            // 获取连续的大小写，数字和下划线的字符
            while ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || (*src == '_'))
                // 计算hash
                token = token * 147 + *src++;
            token = (token << 6) + (src - ch_ptr);
            symbol_ptr = symbol_table;
            // 检查是否存储过
            while (symbol_ptr[Token])
            {
                // 先hash再名字进行比较
                if (token == symbol_ptr[Hash] && !memcmp((char *)symbol_ptr[Name], ch_ptr, src - ch_ptr))
                {
                    // 找到就返回，在解析代码前，会先把keyword跑一遍tokenize
                    token = symbol_ptr[Token];
                    return;
                }
                symbol_ptr = symbol_ptr + SymSize;
            }
            // 如果没存过，就是代码里新有的，进行存储
            symbol_ptr[Name] = (int)ch_ptr;
            symbol_ptr[Hash] = token;
            token = symbol_ptr[Token] = Id;
            return;
        }
        // 解析number
        else if (token >= '0' && token <= '9')
        {
            // 10进制
            if ((token_val = token - '0'))
                while (*src >= '0' && *src <= '9')
                    token_val = token_val * 10 + *src++ - '0';
            // 16进制
            else if (*src == 'x' || *src == 'X')
                while ((token = *++src) && ((token >= '0' && token <= '9') || (token >= 'a' && token <= 'f') || (token >= 'A' && token <= 'F')))
                    token_val = token_val * 16 + (token & 0xF) + (token >= 'A' ? 9 : 0);
            // 8进制
            else
                while (*src >= '0' && *src <= '7')
                    token_val = token_val * 8 + *src++ - '0';
            token = Num;
            return;
        }
        // string or char
        else if (token == '"' || token == '\'')
        {
            ch_ptr = data;
            while (*src != 0 && *src != token)
            {
                // 仅支持\n
                if ((token_val = *src++) == '\\')
                {
                    if ((token_val = *src++) == 'n')
                        token_val = '\n';
                }
                // 字符串要整体储存
                if (token == '"')
                    *data++ = token_val;
            }
            src++;
            // 字符串是指针
            if (token == '"')
                token_val = (int)ch_ptr;
            // char实际上是个数字
            else
                token = Num;
            return;
        }
        // 除法
        else if (token == '/')
        {
            // 注释，全部忽略
            if (*src == '/')
            {
                while (*src != 0 && *src != '\n')
                    src++;
            }
            else
            {
                // 是除法，设置除法的token
                token = Div;
                return;
            }
        }
        // 剩下所有操作符的处理
        else if (token == '=')
        {
            if (*src == '=')
            {
                src++;
                token = Eq;
            }
            else
                token = Assign;
            return;
        }
        else if (token == '+')
        {
            if (*src == '+')
            {
                src++;
                token = Inc;
            }
            else
                token = Add;
            return;
        }
        else if (token == '-')
        {
            if (*src == '-')
            {
                src++;
                token = Dec;
            }
            else
                token = Sub;
            return;
        }
        else if (token == '!')
        {
            if (*src == '=')
            {
                src++;
                token = Ne;
            }
            return;
        }
        else if (token == '<')
        {
            if (*src == '=')
            {
                src++;
                token = Le;
            }
            else if (*src == '<')
            {
                src++;
                token = Shl;
            }
            else
                token = Lt;
            return;
        }
        else if (token == '>')
        {
            if (*src == '=')
            {
                src++;
                token = Ge;
            }
            else if (*src == '>')
            {
                src++;
                token = Shr;
            }
            else
                token = Gt;
            return;
        }
        else if (token == '|')
        {
            if (*src == '|')
            {
                src++;
                token = Lor;
            }
            else
                token = Or;
            return;
        }
        else if (token == '&')
        {
            if (*src == '&')
            {
                src++;
                token = Land;
            }
            else
                token = And;
            return;
        }
        else if (token == '^')
        {
            token = Xor;
            return;
        }
        else if (token == '%')
        {
            token = Mod;
            return;
        }
        else if (token == '*')
        {
            token = Mul;
            return;
        }
        else if (token == '[')
        {
            token = Brak;
            return;
        }
        else if (token == '?')
        {
            token = Cond;
            return;
        }
        else if (token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':')
            return;
    }
}

void assert(int tk)
{
    if (token != tk)
    {
        printf("line %lld: expect token: %lld(%c), get: %lld(%c)\n", line, tk, (char)tk, token, (char)token);
        exit(-1);
    }
    tokenize();
}

void check_local_id()
{
    if (token != Id)
    {
        printf("line %lld: invalid identifer\n", line);
        exit(-1);
    }
    if (symbol_ptr[Class] == Loc)
    {
        printf("line %lld: duplicate declaration\n", line);
        exit(-1);
    }
}

void check_new_id()
{
    if (token != Id)
    {
        printf("line %lld: invalid identifer\n", line);
        exit(-1);
    }
    if (symbol_ptr[Class])
    {
        printf("line %lld: duplicate declaration\n", line);
        exit(-1);
    }
}

// 解析enum
void parse_enum()
{
    int i;
    i = 0;
    while (token != '}')
    {
        check_new_id();
        assert(Id);
        if (token == Assign)
        {
            assert(Assign);
            assert(Num);
            i = token_val;
        }
        symbol_ptr[Class] = Num;
        symbol_ptr[Type] = INT;
        symbol_ptr[Value] = i++;
        if (token == ',')
            tokenize();
    }
}

// 解析int/char
int parse_base_type()
{
    if (token == Char)
    {
        assert(Char);
        return CHAR;
    }
    else
    {
        assert(Int);
        return INT;
    }
}

// 变量屏蔽
void hide_global()
{
    symbol_ptr[GClass] = symbol_ptr[Class];
    symbol_ptr[GType] = symbol_ptr[Type];
    symbol_ptr[GValue] = symbol_ptr[Value];
}

// 变量恢复
void recover_global()
{
    symbol_ptr[Class] = symbol_ptr[GClass];
    symbol_ptr[Type] = symbol_ptr[GType];
    symbol_ptr[Value] = symbol_ptr[GValue];
}

// 解析函数传参
int ibp; // 当前函数base pointer地址
void parse_param()
{
    int type, i;
    i = 0;
    while (token != ')')
    {
        type = parse_base_type();
        while (token == Mul)
        {
            assert(Mul);
            type = type + PTR;
        }
        check_local_id();
        assert(Id);
        hide_global();
        symbol_ptr[Class] = Loc;
        symbol_ptr[Type] = type;
        symbol_ptr[Value] = i++;
        if (token == ',')
            assert(',');
    }
    ibp = ++i;
}

int type;
void parse_expr(int precd)
{
    int tmp_type, i;
    int *tmp_ptr;
    // 处理 number
    if (token == Num)
    {
        tokenize();
        *++code = IMM;
        *++code = token_val;
        type = INT;
    }
    // 处理 string
    else if (token == '"')
    {
        *++code = IMM;
        *++code = token_val; // string addr
        assert('"');
        while (token == '"')
            assert('"');                     // 处理多行
        data = (char *)((int)data + 8 & -8); // add \0 for string & align 8
        type = PTR;
    }
    else if (token == Sizeof)
    {
        tokenize();
        assert('(');
        type = parse_base_type();
        while (token == Mul)
        {
            assert(Mul);
            type = type + PTR;
        }
        assert(')');
        *++code = IMM;
        *++code = (type == CHAR) ? 1 : 8;
        type = INT;
    }
    // 处理 identifer 可能是一个变量或者函数调用
    else if (token == Id)
    {
        tokenize();
        tmp_ptr = symbol_ptr; // 用于递归解析
        // 函数调用
        if (token == '(')
        {
            assert('(');
            i = 0; // 记录 args
            while (token != ')')
            {
                parse_expr(Assign);
                *++code = PUSH;
                i++;
                if (token == ',')
                    assert(',');
            }
            assert(')');
            // native call
            if (tmp_ptr[Class] == Sys)
                *++code = tmp_ptr[Value];
            // 函数调用
            else if (tmp_ptr[Class] == Fun)
            {
                *++code = CALL;
                *++code = tmp_ptr[Value];
            }
            else
            {
                printf("line %lld: invalid function call\n", line);
                exit(-1);
            }
            // delete stack frame for args
            if (i > 0)
            {
                *++code = DARG;
                *++code = i;
            }
            type = tmp_ptr[Type];
        }
        // enum
        else if (tmp_ptr[Class] == Num)
        {
            *++code = IMM;
            *++code = tmp_ptr[Value];
            type = INT;
        }
        // 处理变量
        else
        {
            // 局部变量，其地址依赖于ibp
            if (tmp_ptr[Class] == Loc)
            {
                *++code = LEA;
                *++code = ibp - tmp_ptr[Value];
            }
            // 全局变量
            else if (tmp_ptr[Class] == Glo)
            {
                *++code = IMM;
                *++code = tmp_ptr[Value];
            }
            else
            {
                printf("line %lld: invalid variable\n", line);
                exit(-1);
            }
            type = tmp_ptr[Type];
            *++code = (type == CHAR) ? LC : LI;
        }
    }
    // 强制转换或括号
    else if (token == '(')
    {
        assert('(');
        if (token == Char || token == Int)
        {
            tokenize();
            tmp_type = token - Char + CHAR;
            while (token == Mul)
            {
                assert(Mul);
                tmp_type = tmp_type + PTR;
            }
            // 使用优先级 Inc 代表所有一元运算符
            assert(')');
            parse_expr(Inc);
            type = tmp_type;
        }
        else
        {
            parse_expr(Assign);
            assert(')');
        }
    }
    // 解引用
    else if (token == Mul)
    {
        tokenize();
        parse_expr(Inc);
        if (type >= PTR)
            type = type - PTR;
        else
        {
            printf("line %lld: invalid dereference\n", line);
            exit(-1);
        }
        *++code = (type == CHAR) ? LC : LI;
    }
    // &
    else if (token == And)
    {
        tokenize();
        parse_expr(Inc);
        if (*code == LC || *code == LI)
            code--; // 返回地址
        else
        {
            printf("line %lld: invalid reference\n", line);
            exit(-1);
        }
        type = type + PTR;
    }
    // ！
    else if (token == '!')
    {
        tokenize();
        parse_expr(Inc);
        *++code = PUSH;
        *++code = IMM;
        *++code = 0;
        *++code = EQ;
        type = INT;
    }
    // ~
    else if (token == '~')
    {
        tokenize();
        parse_expr(Inc);
        *++code = PUSH;
        *++code = IMM;
        *++code = -1;
        *++code = XOR;
        type = INT;
    }
    // +
    else if (token == And)
    {
        tokenize();
        parse_expr(Inc);
        type = INT;
    }
    // -
    else if (token == Sub)
    {
        tokenize();
        parse_expr(Inc);
        *++code = PUSH;
        *++code = IMM;
        *++code = -1;
        *++code = MUL;
        type = INT;
    }
    // ++var --var
    else if (token == Inc || token == Dec)
    {
        i = token;
        tokenize();
        parse_expr(Inc);
        // save var addr, then load var val
        if (*code == LC)
        {
            *code = PUSH;
            *++code = LC;
        }
        else if (*code == LI)
        {
            *code = PUSH;
            *++code = LI;
        }
        else
        {
            printf("line %lld: invalid Inc or Dec\n", line);
            exit(-1);
        }
        *++code = PUSH; // 存储到stack
        *++code = IMM;
        *++code = (type > PTR) ? 8 : 1;
        *++code = (i == Inc) ? ADD : SUB;   // 计算
        *++code = (type == CHAR) ? SC : SI; // 写回变量地址
    }
    else
    {
        printf("line %lld: invalid expression\n", line);
        exit(-1);
    }
    // 使用优先级爬山方法来处理二元运算符
    while (token >= precd)
    {
        tmp_type = type;
        // 赋值
        if (token == Assign)
        {
            tokenize();
            if (*code == LC || *code == LI)
                *code = PUSH;
            else
            {
                printf("line %lld: invalid assignment\n", line);
                exit(-1);
            }
            parse_expr(Assign);
            type = tmp_type; // 类型可以强制转换
            *++code = (type == CHAR) ? SC : SI;
        }
        // ? :, if中
        else if (token == Cond)
        {
            tokenize();
            *++code = JZ;
            tmp_ptr = ++code;
            parse_expr(Assign);
            assert(':');
            *tmp_ptr = (int)(code + 3);
            *++code = JMP;
            tmp_ptr = ++code; // 存储 endif 地址
            parse_expr(Cond);
            *tmp_ptr = (int)(code + 1); // 写入 endif 地址
        }
        // 逻辑运算符
        else if (token == Lor)
        {
            tokenize();
            *++code = JNZ;
            tmp_ptr = ++code;
            parse_expr(Land);
            *tmp_ptr = (int)(code + 1);
            type = INT;
        }
        else if (token == Land)
        {
            tokenize();
            *++code = JZ;
            tmp_ptr = ++code;
            parse_expr(Or);
            *tmp_ptr = (int)(code + 1);
            type = INT;
        }
        else if (token == Or)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Xor);
            *++code = OR;
            type = INT;
        }
        else if (token == Xor)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(And);
            *++code = XOR;
            type = INT;
        }
        else if (token == And)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Eq);
            *++code = AND;
            type = INT;
        }
        else if (token == Eq)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Lt);
            *++code = EQ;
            type = INT;
        }
        else if (token == Ne)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Lt);
            *++code = NE;
            type = INT;
        }
        else if (token == Lt)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Shl);
            *++code = LT;
            type = INT;
        }
        else if (token == Gt)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Shl);
            *++code = GT;
            type = INT;
        }
        else if (token == Le)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Shl);
            *++code = LE;
            type = INT;
        }
        else if (token == Ge)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Shl);
            *++code = GE;
            type = INT;
        }
        else if (token == Shl)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Add);
            *++code = SHL;
            type = INT;
        }
        else if (token == Shr)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Add);
            *++code = SHR;
            type = INT;
        }
        // arithmetic operators
        else if (token == Add)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Mul);
            // int pointer * 8
            if (tmp_type > PTR)
            {
                *++code = PUSH;
                *++code = IMM;
                *++code = 8;
                *++code = MUL;
            }
            *++code = ADD;
            type = tmp_type;
        }
        else if (token == Sub)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Mul);
            if (tmp_type > PTR && tmp_type == type)
            {
                // pointer - pointer, ret / 8
                *++code = SUB;
                *++code = PUSH;
                *++code = IMM;
                *++code = 8;
                *++code = DIV;
                type = INT;
            }
            else if (tmp_type > PTR)
            {
                *++code = PUSH;
                *++code = IMM;
                *++code = 8;
                *++code = MUL;
                *++code = SUB;
                type = tmp_type;
            }
            else
                *++code = SUB;
        }
        else if (token == Mul)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Inc);
            *++code = MUL;
            type = INT;
        }
        else if (token == Div)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Inc);
            *++code = DIV;
            type = INT;
        }
        else if (token == Mod)
        {
            tokenize();
            *++code = PUSH;
            parse_expr(Inc);
            *++code = MOD;
            type = INT;
        }
        // var++, var--
        else if (token == Inc || token == Dec)
        {
            if (*code == LC)
            {
                *code = PUSH;
                *++code = LC;
            } // 存储赋值地址
            else if (*code == LI)
            {
                *code = PUSH;
                *++code = LI;
            }
            else
            {
                printf("%lld: invlid operator=%lld\n", line, token);
                exit(-1);
            }
            *++code = PUSH;
            *++code = IMM;
            *++code = (type > PTR) ? 8 : 1;
            *++code = (token == Inc) ? ADD : SUB;
            *++code = (type == CHAR) ? SC : SI; // 存储  ++ or -- 的值到地址
            *++code = PUSH;
            *++code = IMM;
            *++code = (type > PTR) ? 8 : 1;
            *++code = (token == Inc) ? SUB : ADD; // 恢复 ++ 或 -- 之前的值
            tokenize();
        }
        // a[x] = *(a + x)
        else if (token == Brak)
        {
            assert(Brak);
            *++code = PUSH;
            parse_expr(Assign);
            assert(']');
            if (tmp_type > PTR)
            {
                *++code = PUSH;
                *++code = IMM;
                *++code = 8;
                *++code = MUL;
            }
            else if (tmp_type < PTR)
            {
                printf("line %lld: invalid index op\n", line);
                exit(-1);
            }
            *++code = ADD;
            type = tmp_type - PTR;
            *++code = (type == CHAR) ? LC : LI;
        }
        else
        {
            printf("%lld: invlid token=%lld\n", line, token);
            exit(-1);
        }
    }
}

// 逐行解析函数中的语句
void parse_stmt()
{
    int *a;
    int *b;
    if (token == If)
    {
        assert(If);
        assert('(');
        parse_expr(Assign);
        assert(')');
        *++code = JZ;
        b = ++code;   // JZ 后一行为false的跳转地址
        parse_stmt(); // 解析true的语句
        if (token == Else)
        {
            assert(Else);
            *b = (int)(code + 3); // false的跳转地址写入
            *++code = JMP;
            b = ++code;   // JMP后为endif地址
            parse_stmt(); // 解析false的语句
        }
        *b = (int)(code + 1); // 写入endif地址
    }
    else if (token == While)
    {
        assert(While);
        a = code + 1; // 记录loop的地址
        assert('(');
        parse_expr(Assign);
        assert(')');
        *++code = JZ;
        b = ++code; // JZ 后为loop结束的地址
        parse_stmt();
        *++code = JMP;
        *++code = (int)a;     // JMP 后为loop的地址
        *b = (int)(code + 1); // 写入loop结束的地址
    }
    else if (token == Return)
    {
        assert(Return);
        if (token != ';')
            parse_expr(Assign);
        assert(';');
        *++code = RET;
    }
    else if (token == '{')
    {
        assert('{');
        while (token != '}')
            parse_stmt(Assign);
        assert('}');
    }
    else if (token == ';')
        assert(';');
    else
    {
        parse_expr(Assign);
        assert(';');
    }
}

// 解析函数
void parse_fun()
{
    int type, i;
    i = ibp;
    // 局部变量必须在顶部
    while (token == Char || token == Int)
    {
        type = parse_base_type();
        while (token != ';')
        {
            while (token == Mul)
            {
                assert(Mul);
                type = type + PTR;
            }
            check_local_id();
            assert(Id);
            hide_global();
            symbol_ptr[Class] = Loc;
            symbol_ptr[Type] = type;
            symbol_ptr[Value] = ++i;
            if (token == ',')
                assert(',');
        }
        assert(';');
    }
    // 生成的代码中写入NVAR
    *++code = NVAR;
    // 生成的代码中写入为分配参数的地址数
    *++code = i - ibp;
    // 逐行解析语句
    while (token != '}')
        parse_stmt();
    if (*code != RET)
        *++code = RET; // void function
    // 恢复全局变量
    symbol_ptr = symbol_table;
    while (symbol_ptr[Token])
    {
        if (symbol_ptr[Class] == Loc)
            recover_global();
        symbol_ptr = symbol_ptr + SymSize;
    }
}

// 文法解析
void parse()
{
    int type, base_type;
    int *p;
    line = 1;
    token = 1;
    while (token > 0)
    {
        tokenize(); // start or skip last ; | }
        // parse enum
        if (token == Enum)
        {
            assert(Enum);
            if (token != '{')
                assert(Id);
            assert('{');
            parse_enum();
            assert('}');
        }
        else if (token == Int || token == Char)
        {
            base_type = parse_base_type();
            // 解析变量和函数
            while (token != ';' && token != '}')
            {
                type = base_type;
                // 处理指针
                while (token == Mul)
                {
                    assert(Mul);
                    type = type + PTR;
                }
                // 检查是否是新的identifer
                check_new_id();
                assert(Id);
                symbol_ptr[Type] = type;
                // 如果有(则是函数
                if (token == '(')
                {
                    symbol_ptr[Class] = Fun;
                    symbol_ptr[Value] = (int)(code + 1);
                    assert('(');
                    parse_param();
                    assert(')');
                    assert('{');
                    parse_fun();
                }
                else
                {
                    // 全局变量
                    symbol_ptr[Class] = Glo;
                    symbol_ptr[Value] = (int)data;
                    data = data + 8; // 每个变量是64 bits
                }
                // int a,b,c;
                if (token == ',')
                    assert(',');
            }
        }
    }
}

// char *insts;
// void write_as()
// {
//     int fd;
//     char *buffer;
//     insts = "IMM ,LEA ,JMP ,JZ  ,JNZ ,CALL,NVAR,DARG,RET ,LI  ,LC  ,SI  ,SC  ,PUSH,"
//             "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
//             "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT,";
//     fd = open("assemble", 0x0001 | 0x0200);
//     buffer = malloc(100);
//     while (code_dump < code)
//     {
//         sprintf(buffer, "(%lld) %8.4s", ++code_dump, insts + (*code_dump * 5));
//         write(fd, buffer, strlen(buffer));
//         if (*code_dump < RET)
//             sprintf(buffer, " %lld\n", *++code_dump);
//         else
//         {
//             buffer[0] = '\n';
//             buffer[1] = '\0';
//         }
//         write(fd, buffer, strlen(buffer));
//     }
//     close(fd);
// }

int load_src(char *file)
{
    int fd, cnt;
    // use open/read/close for bootstrap.
    if ((fd = open(file, 0)) < 0)
    {
        printf("could not open source code(%s)\n", file);
        return -1;
    }
    if (!(src = src_dump = malloc(MAX_SIZE)))
    {
        printf("could not malloc(%lld) for source code\n", MAX_SIZE);
        return -1;
    }
    if ((cnt = read(fd, src, MAX_SIZE - 1)) <= 0)
    {
        printf("could not read source code(%lld)\n", cnt);
        return -1;
    }
    src[cnt] = 0; // EOF
    close(fd);
    return 0;
}

void keyword()
{
    int i;
    src = "char int enum if else return sizeof while "
          "open read close printf malloc free memset memcmp exit void main";
    i = Char;
    while (i <= While)
    {
        tokenize();
        symbol_ptr[Token] = i++;
    }
    i = OPEN;
    while (i <= EXIT)
    {
        tokenize();
        symbol_ptr[Class] = Sys;
        symbol_ptr[Type] = INT;
        symbol_ptr[Value] = i++;
    }
    tokenize();
    symbol_ptr[Token] = Char;
    tokenize();
    main_ptr = symbol_ptr;
    src = src_dump;
}

int init_vm()
{
    if (!(code = code_dump = malloc(MAX_SIZE)))
    {
        printf("could not malloc(%lld) for code segment\n", MAX_SIZE);
        return -1;
    }
    if (!(data = malloc(MAX_SIZE)))
    {
        printf("could not malloc(%lld) for data segment\n", MAX_SIZE);
        return -1;
    }
    if (!(stack = malloc(MAX_SIZE)))
    {
        printf("could not malloc(%lld) for stack segment\n", MAX_SIZE);
        return -1;
    }
    if (!(symbol_table = malloc(MAX_SIZE / 16)))
    {
        printf("could not malloc(%lld) for symbol_table\n", MAX_SIZE / 16);
        return -1;
    }
    memset(code, 0, MAX_SIZE);
    memset(data, 0, MAX_SIZE);
    memset(stack, 0, MAX_SIZE);
    memset(symbol_table, 0, MAX_SIZE / 16);
    return 0;
}

int main(int argc, char **argv)
{
    MAX_SIZE = 128 * 1024 * 8; // 1MB = 128k * 64bit
    // 加载代码
    if (load_src(*(argv + 1)) != 0)
        return -1;
    // 初始化
    if (init_vm() != 0)
        return -1;
    // 初始化keyword
    keyword();
    parse();
    // write_as();
    return run_vm(--argc, ++argv);
}