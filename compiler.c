#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "next.h"
#include "eval.h"
#include "parse.h"

void global_declaration();
void enum_declaration();
void function_declaration();
void function_parameter();
void function_body();
void statement();
void expression(int level);
void program();

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
long *idmain; // the 'main' function

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
            printf("%d: bad enum identifier %ld\n", line, token);
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
    /*
    1. if (...) <statement> [else <statement>]
    2. while(...) <statement>
    3. { <statement> }
    4. return xxx;
    5. <empty statement>;
    6. expression; (expression end with semicolon)
    7. do{...} while(...)
    */

    long *a, *b; // bess for branch control

    /*
        if (...) <statement> [else <statement>]

        if (<cond>)                   <cond>
                                  JZ a
        <true_statement>   ===>     <true_statement>
        else:                         JMP b
    a:                           a:
        <false_statement>           <false_statement>
    b:                           b:
    */

    if(token == If)
    {
        match(If);
        match('(');
        expression(Assign); // parse condition
        match(')');

        *++text = JZ;
        b = ++text;

        statement(); // parse statement
        if(token == Else)
        {
            // parse else
            match(Else);

            // emit code for JMP B
            *b = (long)(text + 3);
            *++text = JMP;
            b = ++text;

            statement();
        }
        *b = (long)(text + 1);
    }

    else if(token == While)
    {
        /*
        a:                     a:
            while (<cond>)        <cond>
                                  JZ b
            <statement>           <statement>
                                  JMP a
        b:                     b:
        */

        match(While);

        a = text + 1;
        
        match('(');
        expression(Assign);
        match(')');

        *++text = JZ;
        b = ++text;

        statement();
        
        *++text = JMP;
        *++text = (long)a;
        *b = (long)(text + 1);
    }

    else if(token == '{')
    {
        // {<statement> ... }
        match('{');

        while(token != '}') statement();
        match('}');
    }

    else if(token == Return)
    {
        // return [expression]
        match(Return);

        if(token != ';') expression(Assign);
        match(';');

        // emit code for return
        *++text = LEV;
    }

    else if(token == ';') match(';'); // empty statement

    else if(token == Do)
    {
        // do while
        match(Do);
        a = text + 1;

        // statement();
        
        match('{');
        // statements
        while(token != '}') statement();
        match('}');

        match(While);
        match('(');

        expression(Assign);
        match(')');

        match(';');
        *++text = JNZ;
        *++text = (long)a;
    }

    else
    {
        // a = b; or function_call();
        expression(Assign);
        match(';');
    }
}

/* analysis one expression */
void expression(int level)
{
    /*
    after step 1, 2
    |      |
    +------+
    | 3    |   |      |
    +------+   +------+
    | 2    |   | +    |
    +------+   +------+

    after step 4
    |      |   |      |
    +------+   +------+
    | 5    |   | -    |
    +------+   +------+

    after step 7
    |      |
    +------+
    | 5    |
    +------+   +------+
    | 4    |   | *    |
    +------+   +------+
    | 5    |   | -    |
    +------+   +------+
    */

    long *id;
    long tmp;
    long *addr;
    
    if(token == Num)
    {
        match(Num);

        // emit code
        *++text = IMM;
        *++text = token_val;
        expr_type = INT;
    }

    else if(token == '"')
    {
        // emit code
        *++text = IMM;
        *++text = token_val;

        match('"');
        // store the rest strings
        while(token == '"') match('"');

        // append the end of stroing character '\0', all the data are default
        // to 0, so just move data one position forward
        data = (char *)(((long)data + sizeof(long))&(-sizeof(long)));
        expr_type = PTR;
    }

    else if(token == Sizeof)
    {
        // sizeof is actually an unary operator
        // now only `sizeof(long)`, `sizeof(char)` and `sizeof(*...)` are
        // supported
        match(Sizeof);
        match('(');
        expr_type = INT;

        if(token == Int) match(Int);
        else if(token == Char)
        {
            match(Char);
            expr_type = CHAR;
        }

        while(token == Mul)
        {
            match(Mul);
            expr_type += PTR;
        }

        match(')');

        // emit code
        *++text = IMM;
        *++text = (expr_type == CHAR) ? sizeof(char) : sizeof(long);

        expr_type = INT;
    }

    else if (token == Id)
    {
        // there are several type when occurs to ID
        // but this is unit, so it can only be
        // 1. function call
        // 2. Enum variable
        // 3. global/local variable
        match(Id);

        id = current_id;

        if(token == '(')
        {
            // function call
            match('(');

            // pass in arguments
            tmp = 0; // number of arguments
            while(token != ')')
            {
                expression(Assign);
                *++text = PUSH;
                tmp++;

                if(token == ',') match(',');
            }
            match(')');

            // emit code
            if(id[Class] == Sys) *++text = id[Value]; // system functions
            else if(id[Class] == Fun)
            {
                // function call
                *++text = CALL;
                *++text = id[Value];
            }
            
            else
            {
                printf("%d: bad function call\n", line);
                exit(-1);
            }

            // clean the stack for arguments
            if(tmp > 0)
            {
                *++text = ADJ;
                *++text = tmp;
            }
            expr_type = id[Type];
        }

        else if(id[Class] == Num)
        {
            // enum variable
            *++text = IMM;
            *++text = id[Value];
            expr_type = INT;
        }

        else
        {
            // variable
            if(id[Class] == Loc)
            {
                *++text = LEA;
                *++text = index_of_bp - id[Value];
            }

            else if(id[Class] == Glo)
            {
                *++text = IMM;
                *++text = id[Value];
            }

            else
            {
                printf("%d: undefined variable\n", line);
                exit(-1);
            }

            // emit code, default behaviour is to load the value of the
            // address which is stored in `ax`
            expr_type = id[Type];
            *++text = (expr_type == Char) ? LC : LI;
        }
    }

    else if(token == '(')
    {
        // cast or parenthesis
        match('(');
        if(token == Int || token == Char)
        {
            tmp = (token == Char) ? CHAR : INT; // cast type
            match(token);
            while(token == Mul)
            {
                match(Mul);
                tmp += PTR;
            }

            match(')');

            expression(Inc); // cast has precedence as Inc(++)

            expr_type = tmp;
        }

        else
        {
            // normal parenthesis
            expression(Assign);
            match(')');
        }
    }

    else if(token == Mul)
    {
        // dereference *<addr>
        match(Mul);
        expression(Inc); // dereference has the same precedence as Inc(++)

        if(expr_type >= PTR) expr_type -= PTR;
        else
        {
            printf("%d: bad dereference\n", line);
            exit(-1);
        }

        *++text = (expr_type == Char) ? LC : LI;
    }

    else if(token == And)
    {
        // get the address of
        match(And);
        expression(Inc); // get the address of
        if(*text == LC || *text == LI) text --;
        else
        {
            printf("%d: bad address of\n", line);
            exit(-1);
        }
        
        expr_type += PTR;
    }

    else if(token == '!')
    {
        // not
        match('!');
        expression(Inc);

        // emit code, use <expr> == 0
        *++text = PUSH;
        *++text = IMM;
        *++text = 0;
        *++text = EQ;

        expr_type = INT;
    }

    else if(token == '~')
    {
        // bitwise not
        match('~');
        expression(Inc);

        // emit code, use <expr> XOR - 1
        *++text = PUSH;
        *++text = IMM;
        *++text = -1;
        *++text = XOR;

        expr_type = INT;
    }

    else if(token == Add)
    {
        // +var, do nothing
        match(Add);
        expression(Inc);
        
        expr_type = INT;
    }

    else if(token == Sub)
    {
        // -var
        match(Sub);

        if(token == Num)
        {
            *++text = IMM;
            *++text = -token_val;
            match(Num);
        }

        else
        {
            *++text = IMM;
            *++text = -1;
            *++text = PUSH;
            expression(Inc);
            *++text = MUL;
        }

        expr_type = INT;
    }

    else if(token == Inc || token == Dec)
    {
        tmp = token;
        match(token);
        expression(Inc);

        if(*text == LC)
        {
            *text = PUSH; // to duplicate the address
            *++text = LC;
        }

        else if(*text == LI)
        {
            *text = PUSH;
            *++text = LI;
        }

        else
        {
            printf("%d: bad lvalue of pre-increment\n", line);
            exit(-1);
        }

        *++text = PUSH;
        *++text = IMM;

        *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
        *++text = (tmp == Inc) ? ADD : SUB;
        *++text = (expr_type ==CHAR) ? SC : SI;
    }

    else
    {
        printf("%d: bad expression\n", line);
        exit(-1);
    }

    while(token >= level)
    {
        // parse token for binary operator and postfix operator
        // IMM <addr>
        // LC/LI

        // IMM <addr>
        // PUSH
        // SC/SI

        tmp = expr_type;
        if(token == Assign)
        {
            // var = expr
            match(Assign);
            if(*text == LC || *text == LI) *text = PUSH; // save the lvalue's pointer
            else
            {
                printf("%d: bad lvalue in assignment\n", line);
                exit(-1);
            }
            expression(Assign);

            expr_type = tmp;
            *++text = (expr_type == CHAR) ? SC : SI;
        }

        else if(token == Cond)
        {
            // expr ? a : b;
            match(Cond);
            *++text = JZ;
            addr = ++text;
            expression(Assign);
            if(token == ':') match(':');
            else
            {
                printf("%d: missing colon in conditional\n", line);
                exit(-1);
            }
            *addr = (long)(text + 3);
            *++text = JMP;
            addr = ++text;
            expression(Cond);
            *addr = (long)(text + 1);
        }

        /*
        <expr1> || <expr2>     <expr1> && <expr2>

            ...<expr1>...          ...<expr1>...
            JNZ b                  JZ b
            ...<expr2>...          ...<expr2>...
        b:                     b:
        */

        else if(token == Lor)
        {
            // Logic or
            match(Lor);
            *++text = JNZ;
            addr = ++text;
            expression(Lan);
            *addr = (long)(text + 1);
            expr_type = INT;
        }

        else if(token == Lan)
        {
            // Logic and
            match(Lan);
            *++text = JZ;
            addr = ++text;
            expression(Or);
            *addr = (long)(text + 1);
            expr_type = INT;
        }

        /*
        <expr1> ^ <expr2>

        ...<expr1>...          <- now the result is on ax
        PUSH
        ...<expr2>...          <- now the value of <expr2> is on ax
        XOR 
        */

        else if(token == Or)
        {
            // bitwise or
            match(Or);
            *++text = PUSH;
            expression(Xor);
            *++text = OR;
            expr_type = INT;
        }

        else if(token == Xor)
        {
            // bitwise xor
            match(Xor);
            *++text = PUSH;
            expression(And);
            *++text = XOR;
            expr_type = INT;
        }

        else if (token == And) 
        {
            // bitwise and
            match(And);
            *++text = PUSH;
            expression(Eq);
            *++text = AND;
            expr_type = INT;
        }
        
        else if (token == Eq)
        {
            // equal ==
            match(Eq);
            *++text = PUSH;
            expression(Ne);
            *++text = EQ;
            expr_type = INT;
        }
        
        else if (token == Ne)
        {
            // not equal !=
            match(Ne);
            *++text = PUSH;
            expression(Lt);
            *++text = NE;
            expr_type = INT;
        }
            
        else if (token == Lt)
        {
            // less than
            match(Lt);
            *++text = PUSH;
            expression(Shl);
            *++text = LT;
            expr_type = INT;
        }
        
        else if (token == Gt)
        {
            // greater than
            match(Gt);
            *++text = PUSH;
            expression(Shl);
            *++text = GT;
            expr_type = INT;
        }
            
        else if (token == Le)
        {
            // less than or equal to
            match(Le);
            *++text = PUSH;
            expression(Shl);
            *++text = LE;
            expr_type = INT;
        }
            
        else if (token == Ge)
        {
            // greater than or equal to
            match(Ge);
            *++text = PUSH;
            expression(Shl);
            *++text = GE;
            expr_type = INT;
        }
            
        else if (token == Shl)
        {
            // shift left
            match(Shl);
            *++text = PUSH;
            expression(Add);
            *++text = SHL;
            expr_type = INT;
        }
            
        else if (token == Shr)
        {
            // shift right
            match(Shr);
            *++text = PUSH;
            expression(Add);
            *++text = SHR;
            expr_type = INT;
        }

        /*
        <expr1> + <expr2>

        normal         pointer

        <expr1>        <expr1>
        PUSH           PUSH
        <expr2>        <expr2>     |
        ADD            PUSH        | <expr2> * <unit>
                       IMM <unit>  |
                       MUL         |
                       ADD
        */

        else if(token == Add)
        {
            // add
            match(Add);
            *++text = PUSH;
            expression(Mul);

            expr_type = tmp;
            if(expr_type > PTR)
            {
                // pointer type, and not `char*`
                *++text = PUSH;
                *++text = IMM;
                *++text = sizeof(long);
                *++text = MUL;
            }
            *++text = ADD;
        }

        else if(token == Sub)
        {
            // sub
            match(Sub);
            *++text = PUSH;
            expression(Mul);
            if(tmp > PTR && tmp == expr_type)
            {
                // pointer substraction
                *++text = SUB;
                *++text = PUSH;
                *++text = IMM;
                *++text = sizeof(long);
                *++text = DIV;
                expr_type = INT;
            }
            
            else if(tmp > PTR)
            {
                // pointer movement
                *++text = PUSH;
                *++text = IMM;
                *++text = sizeof(long);
                *++text = MUL;
                *++text = SUB;
                expr_type = tmp;
            }

            else
            {
                // numeral substraction
                *++text = SUB;
                expr_type = tmp;
            }
        }

        else if (token == Mul)
        {
            // multiply
            match(Mul);
            *++text = PUSH;
            expression(Inc);
            *++text = MUL;
            expr_type = tmp;
        }
            
        else if (token == Div)
        {
            // divide
            match(Div);
            *++text = PUSH;
            expression(Inc);
            *++text = DIV;
            expr_type = tmp;
        }
            
        else if (token == Mod)
        {
            // Modulo
            match(Mod);
            *++text = PUSH;
            expression(Inc);
            *++text = MOD;
            expr_type = tmp;
        }

        else if(token == Inc || token == Dec)
        {
            // postfix inc(++) and dec(--)
            // we will increase the value to the variable and decrease it
            // on `ax` to get its original value
            if(*text == LI)
            {
                *text = PUSH;
                *++text = LI;
            }

            else if(*text == LC)
            {
                *text = PUSH;
                *++text = LC;
            }

            else
            {
                printf("%d: bad value in increment\n", line);
                exit(-1);
            }

            *++text = PUSH;
            *++text = IMM;
            *++text = (expr_type > PTR) ? sizeof(long) : sizeof(char);
            *++text = (token == Inc) ? ADD : SUB;
            *++text = (expr_type == CHAR) ? SC : SI;

            *++text = PUSH;
            *++text = IMM;
            *++text = (expr_type > PTR) ? sizeof(long) : sizeof(char);
            *++text = (token == Inc) ? SUB : ADD;
            match(token);
        }

        else if(token == Brak)
        {
            // array access var[xx]
            match(Brak);
            *++text = PUSH;
            expression(Assign);
            match(']');

            if(tmp > PTR)
            {
                // pointer, `not char* `
                *++text = PUSH;
                *++text = IMM;
                *++text = sizeof(long);
                *++text = MUL;
            }

            else if(tmp < PTR)
            {
                printf("%d: pointer type expected\n", line);
                exit(-1);
            }

            expr_type = tmp - PTR;
            *++text = ADD;
            *++text = (expr_type == CHAR) ? LC : LI;
        }
    }
}



/* entrance of grammar analysis, analyzing the whole c-file*/
void program()
{
    next(); // get next token
    while(token > 0) global_declaration();
}

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

    src = "char else enum if int return sizeof while "
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