#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

enum { LC, LI, SC, SI,
       IMM, PUSH, JMP, JZ, JNZ, CALL, ENT, ADJ, LEV, LEA, 
       OR, XOR, AND, EQ, NE, LT, LE, GT, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
       EXIT, OPEN, CLOS, READ, PRTF, MALC, MSET, MCMP
};

enum {
    Token, Hash, Name, Type, Class, Value, BType, BClass, BValue, IdSize
};

enum {
    Num = 128, Fun, Sys, Glo, Loc, Id,
          Char, Else, Enum, If, Int, Return, Sizeof, While, Do,
          Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak 
};

enum {CHAR, INT, PTR};

extern long token;
extern char *src, *old_src;
extern int poolsize, line;
extern long *text, *old_text, *stack;
extern char *data;
extern long *pc, *sp, *bp, ax, cycle, token_val, *current_id, *symbols, *idmain;
extern char *line2;
extern char *src;
extern long basetype, expr_type, index_of_bp;

void next();
int eval();
void program();
#endif