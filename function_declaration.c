#include <stdio.h>
#include <stdlib.h>
#include "function_declaration.h"
#include "next.h"
#include "variable_declaration.h"
#include "eval.h"
#include "match.h"
#include "statement.h"

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