#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "next.h"
#include "eval.h"
#include "parse.h"

/*
change most var from int to long to fit my 64-bit machine
*/

long token; // current token
char *src, *old_src; // pointer to source code string
int poolsize; // default size fo text/data/stack
int line; // line number

long *text; // text segment
long *old_text; // for dump next segment
long *stack; // stack
char *data; // data segment
/*
+------------------------+
|   stack |              |
|   ...   \/             |
|                        |
|                        |
|                        |
|   ...  /\              |
|   heap  |              |
+------------------------+
|   bss  segment         |
+------------------------+
|   data  segment        |
+------------------------+
|   text  segment        |
+------------------------+
*/

// Registers 

long *pc; // progrom counting, address of memory, inside of the address is the command for next execution
long *sp; // pointer->top of stack, lowering
long *bp; // base pointer -> some place of the stack, function() will use it
long ax; // store the result after executing one command
long cycle;

// Instructions
/*
enum { LC, LI, SC, SI,
       IMM, PUSH, JMP, JZ, JNZ, CALL, ENT, ADJ, LEV, LEA, 
       OR, XOR, AND, EQ, NE, LT, LE, GT, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
       EXIT, OPEN, CLOS, READ, PRTF, MALC, MSET, MCMP
};
*/


/*
                   +-------+                      +--------+
-- source code --> | lexer | --> token stream --> | parser | --> assembly
                   +-------+                      +--------+
*/

// tokens and classes
/*
enum {
    Num = 128, Fun, Sys, Glo, Loc, Id,
          Char, Else, Enum, If, Int, Return, Sizeof, While,
          Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak 
};
*/


/*
Symbol table:
----+-----+----+----+----+-----+-----+-----+------+------+----
 .. |token|hash|name|type|class|value|btype|bclass|bvalue| .. 
----+-----+----+----+----+-----+-----+-----+------+------+----
    |<--         one single identifier                -->|
*/

long token_val; // value of current token (mainly for number)
long *current_id; // current parsed ID
long *symbols; // symbol table

// fields of identifier
/*
enum {
    Token, Hash, Name, Type, Class, Value, Btype, Bclass, Bvalue, IdSize
};
*/


// types of variable/function
enum {CHAR, INT, PTR};
int *idmain; // the 'main' function

char *line2 = NULL;
char *src = NULL;

void match(long tk)
{
    if(token != tk) 
    {
        printf("expected token: %ld(%ld), got: %ld(%ld)", tk, tk, token, token);
        exit(-1);
    }
    next();
}

/* analysis one expression */
void expression(int level)
{
    // do nothing ~
}
 /* entrance of grammar analysis, analyzing the whole c-file*/
void program()
{
    next(); // get next token
    while(token > 0)
    {
        printf("token is : %ld\n", token);
        next();
    }
}

int main(int argc, char **argv)
{
    size_t linecap = 0;
    ssize_t linelen;
    while((linelen = getline(&line2, &linecap, stdin)) > 0)
    {
        src = line2;
        next();
        printf("%d\n", expr());
    }
    return 0;
    /*
    int i, fd;
    
    argc--;
    argv++;

    poolsize = 256 * 1024; // arbitary size
    line = 1;

    if((fd = open(*argv, 0)) < 0)
    {
        printf("could not open (%s)\n", *argv);
        return -1;
    }

    if(!(src = old_src = malloc(poolsize)))
    {
        printf("could not malloc (%d) for source area\n", poolsize);
        return -1;
    }

    // read the source file
    if((i = read(fd, src, poolsize - 1)) <= 0)
    {
        printf("read() returned %d\n", i);
        return -1;
    }

    src[i] = 0; // add EOF character
    close(fd);

    // allocate memory for virtual machine
    if(!(text = old_text = malloc(poolsize)))
    {
        printf("could not malloc(%d) size for text area", poolsize);
        return -1;
    }

    if(!(data = malloc(poolsize)))
    {
        printf("could not malloc(%d) size for data area", poolsize);
        return -1;
    }

    if(!(stack = malloc(poolsize)))
    {
        printf("could not malloc(%d) size for stack area", poolsize);
        return -1;
    }

    memset(text, 0, poolsize); // The memset() fills a range of memeory in the same value(0)
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);

    bp = sp = (long *)((long)stack + poolsize);
    ax = 0;

    src = "char else enum if int return sizeof while "
    "open read close printf malloc memeset memcmp exit void main";

    // add keywords to symbol table
    i = Char;
    while(i <= While)
    {
        next();
        current_id[Token] = i++;
    }

    // add library to symbol table
    i = OPEN;
    while(i <= EXIT)
    {
        next();
        current_id[Class] = Sys;
        current_id[Type] = INT;
        current_id[Value] = i++;
    }

    next(); current_id[Token] = Char; // handle void type
    next(); idmain = current_id; // keep track of main
    // test "10 + 20"
    // i = 0;
    // text[i++] = IMM;
    // text[i++] = 10;
    // text[i++] = PUSH;
    // text[i++] = IMM;
    // text[i++] = 20;
    // text[i++] = ADD;
    // text[i++] = PUSH;
    // text[i++] = EXIT;
    // pc = text;
    // program();
    return eval();
    */
}