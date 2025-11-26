#ifndef NEXT_H
#define NEXT_H

enum {
    Token, Hash, Name, Type, Class, Value, Btype, Bclass, Bvalue, IdSize
};

enum {
    Num = 128, Fun, Sys, Glo, Loc, Id,
          Char, Else, Enum, If, Int, Return, Sizeof, While,
          Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak 
};

extern int line;
extern long token;
extern char *src;
extern long *current_id;
extern long *symbols;
extern long token_val;
extern char *data;

void next();

#endif