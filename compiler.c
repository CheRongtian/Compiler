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

/* definition of variable */
void match(long tk)
{
    if(token != tk) 
    {
        printf("expected token: %ld(%ld), got: %ld(%ld)", tk, tk, token, token);
        exit(-1);
    }
    next();
}

long basetype; // the type of a declaration, make it global for convenience
long expr_type; // the type of an expression

void global_declaration()
{
    // global_declaration ::= enum_decl | variable_decl | function_decl
    // enum_decl ::= 'enum'[id] '{' id ['=' 'num'} '}'
    // variable_decl ::= type {'*'} id { ',' {'*'} id } ';'
    // function_decl ::= type {'*'} id '(' parameter_decl ')' '{' body_decl '}'

    long type; // tmp, actual type for variable
    long i; // tmp

    basetype = INT;

    // parse enum, this should be treated alone

    if(token == Enum)
    {
        // enum[id] { a = 10, b = 20, ... }
        match(Enum);
        if(token != '{') match(Id); // skip the [id] part
        if(token == '{')
        {
            // parse the assign part
            match('{');
            enum_declaration();
            match('}');
        }
        match(';');
        return;
    }

    // parse type information
    if(token == Int) match(Int);
    else if(token == Char)
    {
        match(Char);
        basetype = CHAR;
    }

    // parse the comma seperated variable declaration
    while(token != ';' && token != '}')
    {
        type = basetype;
        // parse pointer type, note that there may exist `int ****;`
        while(token == Mul)
        {
            match(Mul);
            type += PTR;
        }

        if(token != Id)
        {
            // invalid declaration
            printf("%d: bad global declaration\n", line);
            exit(-1);
        }
        
        if(current_id[Class])
        {
            // identifier exists
            printf("%d: duplicate global declaration\n", line);
            exit(-1);
        }

        match(Id);
        current_id[Type] = type;

        if(token == '(')
        {
            current_id[Class] = Fun;
            current_id[Value] = (long)(text+1); // the memory address of function
            function_declaration();
        }
        
        else
        {
            // variable declaration
            current_id[Class] = Glo; // global variable
            current_id[Class] = (long)data; // assign memory address
            data += sizeof(long);
        }

        if(token == ',') match(',');
    }
    next();
}

void enum_declaration()
{
    // parse enum[id] { a = 1, b = 3, ...}
    long i = 0;
    while(token!= '}')
    {
        if(token != Id)
        {
            printf("%d: bad enum identifier %d\n", line, token);
            exit(-1);
        }
        next();
        if(token == Assign)
        {
            // like {a = 10}
            next();
            if(token != Num)
            {
                printf("%d: bad enum initializr\n", line);
                exit(-1);
            }
            i = token_val;
            next();
        }

        current_id[Class] = Num;
        current_id[Type] = INT;
        current_id[Value] = i++;

        if(token == ',') next();
    }
}

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

void function_declaration()
{
    // type func_name (...) {...}
    //               | this part

    match('(');
    function_parameter();
    match(')');
    match('{');
    function_body();

    // unwind local variable declarations for all local variables
    current_id = symbols;
    while(current_id[Token])
    {
        if(current_id[Class] == Loc)
        {
            current_id[Class] = current_id[BClass];
            current_id[Type] = current_id[BType];
            current_id[Value] = current_id[BValue];
        }
        current_id += IdSize;
    }
}

long index_of_bp; // index of bp pointer on stack

void function_parameter()
{
    long type;
    long params = 0;
    while(token != ')')
    {
        type = INT;
        if(token == Int) match(Int);
        else if(token == Char)
        {
            type = CHAR;
            match(Char);
        }

        // pointer type
        while(token == Mul)
        {
            match(Mul);
            type += PTR;
        }

        // parameter name
        if(token != Id)
        {
            printf("%d: bad parameter declaration\n", line);
            exit(-1);
        }
        
        if(current_id[Class] == Loc)
        {
            printf("%d: duplicate parameter declaration\n", line);
            exit(-1);
        }

        match(Id);

        // store the local variable
        current_id[BClass] = current_id[Class];
        current_id[Class] = Loc;

        current_id[BType] = current_id[Type];
        current_id[Type] = type;

        current_id[BValue] = current_id[Value];
        current_id[Value] = params++; //index of current parameter

        if(token == ',') match(',');
        index_of_bp = params + 1;
    }
}

void function_body()
{
    // type func_name (...) {...}
    //                   -->|   |<--

    // ... {
    // 1. local declarations
    // 2. statements
    // }

    long pos_local; // position of local variables on the stack
    long type;
    pos_local = index_of_bp;

    while(token == Int || token == Char)
    {
        // local variable declaration, just like global ones
        basetype = (token == Int) ? INT : CHAR;
        match(token);

        while(token != ';')
        {
            type = basetype;
            while(token == Mul)
            {
                match(Mul);
                type += PTR;
            }

            if(token != Id)
            {
                // invalid declaration
                printf("%d: bad local declaration\n", line);
                exit(-1);
            }

            if(current_id[Class] == Loc)
            {
                // identifier exists
                printf("%d: duplicate local declaration\n", line);
                exit(-1);
            }

            match(Id);

            // store the local variable
            current_id[BClass] = current_id[Class];
            current_id[Class] = Loc;

            current_id[BType] = current_id[Type];
            current_id[Type] = type;

            current_id[BValue] = current_id[Value];
            current_id[Value] = ++pos_local; //index of current parameter

            if(token == ',') match(',');
        }

        match(';');
    }

    // save the stack size for local variables
    *++text = ENT;
    *++text = pos_local - index_of_bp;

    // statements
    while(token != '}') statement();

    // emit code for leaving the sub function
    *++text = LEV;
}

void statement()
{
    ;
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
    /*
    while(token > 0)
    {
        printf("token is : %ld\n", token);
        next();
    }
    */
   global_declaration();
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