#include <stdio.h>
#include <stdlib.h>
#include "variable_declaration.h"
#include "next.h"
#include "match.h"
#include "function_declaration.h"

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

