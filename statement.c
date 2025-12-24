#include "statement.h"
#include "eval.h"
#include "next.h"
#include "match.h"
#include "expression.h"

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