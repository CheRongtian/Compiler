#include <stdio.h>
#include <stdlib.h>
#include "expression.h"
#include "next.h"
#include "eval.h"
#include "variable_declaration.h"
#include "match.h"

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