#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "next.h"
#include "eval.h"
#include "parse.h"
#include "program.h"
#include "variable_declaration.h"
#include "function_declaration.h"
#include "statement.h"
#include "expression.h"

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

/*
                   +-------+                      +--------+
-- source code --> | lexer | --> token stream --> | parser | --> assembly
                   +-------+                      +--------+
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

long *idmain; // the 'main' function

char *line2 = NULL;
char *src = NULL;

long basetype; // the type of a declaration, make it global for convenience
long expr_type; // the type of an expression

/* definition of function*/
/*
|    ....       | high address
+---------------+
| arg: param_a  |    new_bp + 3
+---------------+
| arg: param_b  |    new_bp + 2
+---------------+
|return address |    new_bp + 1
+---------------+
| old BP        | <- new BP
+---------------+
| local_1       |    new_bp - 1
+---------------+
| local_2       |    new_bp - 2
+---------------+
|    ....       |  low address
*/

long index_of_bp; // index of bp pointer on stack

int main(int argc, char **argv)
{
    int i, fd;
    long *tmp;
    
    argc--;
    argv++;

    poolsize = 256 * 1024; // arbitary size
    line = 1;

    // allocate memory for virtual machine
    if(!(text = old_text = malloc(poolsize)))
    {
        printf("could not malloc(%d) for text area", poolsize);
        return -1;
    }

    if(!(data = malloc(poolsize)))
    {
        printf("could not malloc(%d) for data area", poolsize);
        return -1;
    }

    if(!(stack = malloc(poolsize)))
    {
        printf("could not malloc(%d) for stack area", poolsize);
        return -1;
    }

    if(!(symbols = malloc(poolsize)))
    {
        printf("could not malloc(%d) for symbol table\n", poolsize);
        return -1;
    }

    memset(text, 0, poolsize); // The memset() fills a range of memeory in the same value(0)
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);
    memset(symbols, 0, poolsize);

    bp = sp = (long *)((long)stack + poolsize);
    ax = 0;

    src = "char else enum if int return sizeof while do "
          "open read close printf malloc memset memcmp exit void main";

    // add keywords to symbol table
    i = Char;
    while(i <= Do)
    {
        next();
        current_id[Token] = i++;
    }

    /*
    // add library to symbol table
    i = OPEN;
    while(i <= EXIT)
    {
        next();
        current_id[Class] = Sys;
        current_id[Type] = INT;
        current_id[Value] = i++;
    }
    */
    i = OPEN;
    next(); current_id[Class] = Sys; current_id[Type] = INT; current_id[Value] = OPEN; // open
    next(); current_id[Class] = Sys; current_id[Type] = INT; current_id[Value] = READ; // read
    next(); current_id[Class] = Sys; current_id[Type] = INT; current_id[Value] = CLOS; // close
    next(); current_id[Class] = Sys; current_id[Type] = INT; current_id[Value] = PRTF; // printf
    next(); current_id[Class] = Sys; current_id[Type] = INT; current_id[Value] = MALC; // malloc
    next(); current_id[Class] = Sys; current_id[Type] = INT; current_id[Value] = MSET; // memset
    next(); current_id[Class] = Sys; current_id[Type] = INT; current_id[Value] = MCMP; // memcmp
    next(); current_id[Class] = Sys; current_id[Type] = INT; current_id[Value] = EXIT; // exit
    

    next(); current_id[Token] = Char; // handle void type
    next(); idmain = current_id; // keep track of main

    // read the source file 
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

    program();

    if (!(pc = (long *)idmain[Value])) {
        printf("main() not defined\n");
        return -1;
    }

    // setup stack
    sp = (long*)((long)stack + poolsize);
    *--sp = EXIT; // call exit if main returns
    long *exit_addr = sp;
    *--sp = PUSH;
    tmp = sp;
    *--sp = argc;
    *--sp = (long)argv;
    *--sp = (long)exit_addr;
    // *--sp = (long)tmp;

    return eval();
}