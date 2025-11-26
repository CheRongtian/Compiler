#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

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
enum { LC, LI, SC, SI,
       IMM, PUSH, JMP, JZ, JNZ, CALL, ENT, ADJ, LEV, LEA, 
       OR, XOR, AND, EQ, NE, LT, LE, GT, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD,
       EXIT, OPEN, CLOS, READ, PRTF, MALC, MSET, MCMP
};

/*
                   +-------+                      +--------+
-- source code --> | lexer | --> token stream --> | parser | --> assembly
                   +-------+                      +--------+
*/

// tokens and classes
enum {
    Num = 128, Fun, Sys, Glo, Loc, Id,
          Char, Else, Enum, If, Int, Return, Sizeof, While,
          Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak 
};

/*
Symbol table:
----+-----+----+----+----+-----+-----+-----+------+------+----
 .. |token|hash|name|type|class|value|btype|bclass|bvalue| .. 
----+-----+----+----+----+-----+-----+-----+------+------+----
    |<--         one single identifier                -->|
*/

long token_val; // value of current token (mainly for number)
long *current_id; // current parsed ID
*symbols; // symbol table

// fields of identifier
enum {
    Token, Hash, Name, Type, Class, Value, Btype, Bclass, Bvalue, IdSize
};

// types of variable/function
enum {CHAR, INT, PTR};
int *idmain; // the 'main' function

/* get next one, ignore blanket */
void next()
{
    char *last_pos;
    int hash;
    while (token = *src) 
    {
        *src++; // parse token here, use while to jump unknown token
        if(token == '\n') ++line; // switch lines
        else if (token == '#')
        {
            // do not support macro, just skip it
            while(*src != 0 && *src != '\n') src++;
        }
    
        else if ((token >= 'a' && token <= 'z') || (token <= 'A' && token <= 'Z') || (token == '_'))
        {
            // parser identifier
            last_pos = src - 1;
            hash = token;

            while((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || (*src == '_'))
            {
                hash = hash * 167 + *src; // make mapping result in the output space sparse && uniform as possible
                src++;
            }

            // look for existing identifier, linear search
            current_id = symbols;
            while(current_id[Token])
            {
                if(current_id[Hash] == hash && !memcmp((char *) current_id[Name], last_pos, src-last_pos))
                {
                    // found one return
                    token = current_id[Token];
                    return;
                }
                current_id = current_id + IdSize;
            }
        
            // store new ID
            current_id[Name] = (long) last_pos;
            current_id[Hash] = hash;
            token = current_id[Hash] = Id;
            return;
        }

        else if(token >= '0' && token <= '9')
        {
            // parse number, three kinds: dec(123) hex(0x123) oct(017)
            token_val = token - '0';
            if(token_val > 0)
            {
                // dec, starts with [1-9]
                while(*src >= '0' && *src <= '9') token_val = token_val * 10 + *src++ - '0';
            }
            else
            {
                // starts with number 0
                if(*src == 'x' || *src == 'X')
                {
                    // hex
                    token = *++src;
                    while((token >= '0' && token <= '9') || (token >= 'a' && token <= 'f') || (token >= 'A' && token <= 'F'))
                    {
                        token_val = token_val * 16 + (token & 15) + (token >= 'A' ? 9 : 0);
                        token = *++src;
                    }
                }
                else
                {
                    // oct
                    while(*src >= '0' && *src <= '7') token_val = token_val * 8 + *src++ - '0';
                }
            }
            token = Num;
            return;
        }

        else if(token =='"' || token == '\'')
        {
            // parse string literal, currently, the only supported escape
            // character is '\n', store the string literal into data
            last_pos = data;
            while(*src != 0 && *src != token)
            {
                token_val = *src++;
                if(token_val == '\\')
                {
                    // escape character
                    token_val = *src++;
                    if(token_val == '\n') token_val = '\n';
                }
                if(token == '"') *data++ = token_val;
            }

            src++;
            // if it is a single character, return Num token
            if(token == '"') token_val = (long)last_pos;
            else token = Num;
            return;
        }

        else if(token == '"' || token == '\'')
        {
            // parse string literal, currently, the only supported escape
            // character is '\n', store the string literal into data
            last_pos = data;
            while(*src != 0 && *src != token)
            {
                token_val = *src++;
                if(token_val == '\\')
                {
                    // escape character
                    token_val = *src++;
                    if(token_val = 'n') token_val = '\n';
                }
                if(token == '"') *data++ = token_val;
            }

            src++;
            // if it is a single character, return Num token
            if(token == '"') token_val = (long)last_pos;
            else token = Num;
            return;
        }

        else if (token == '/')
        {
            if(*src == '/')
            {
                // skip comments
                while(*src != 0 && *src != '\n') ++src;
            }
            else
            {
                // divide operator
                token = Div;
                return;
            }
        }

        else if(token == '=')
        {
            // parse '==' and '='
            if(*src == '=')
            {
                src ++;
                token = Eq;
            }
            else token = Assign;
            return;
        }

        else if(token == '+')
        {
            // parse '++' and '+'
            if(*src == '+')
            {
                src ++;
                token = Inc;
            }
            else token = Add;
            return;
        }

        else if(token == '-')
        {
            // parse '--' and '-'
            if(*src == '-')
            {
                src ++;
                token = Dec;
            }
            else token = Sub;
            return;
        }

        else if(token == '!')
        {
            // parse '!='
            if(*src == '=')
            {
                src ++;
                token = Ne;
            }
            return;
        }

        else if(token == '<')
        {
            // parse '<=', '<<' or '<'
            if(*src == '=')
            {
                src ++;
                token = Le;
            }
            else if(*src == '<')
            {
                src ++;
                token = Shl;
            }
            else
            {
                token = Lt;
            }
            return;
        }

        else if(token == '>')
        {
            // parse '>=', '>>' or '>'
            if(*src == '=')
            {
                src ++;
                token = Ge;
            }
            else if (*src == '>')
            {
                src ++;
                token = Shr;
            }
            else token = Gt;
            return;
        }

        else if(token == '|')
        {
            // parse '|' or '||'
            if(*src == '|')
            {
                src ++;
                token = Lor;
            }
            else token = Or;
            return;
        }

        else if(token == '&')
        {
            // parse '&' and '&&'
            if(*src == '&')
            {
                src ++;
                token = Lan;
            }
            else token = And;
            return;
        }

        else if(token == '^')
        {
            token = Xor;
            return;
        }

        else if(token == '%')
        {
            token = Mod;
            return;
        }

        else if(token == '*')
        {
            token = Mul;
            return;
        }

        else if(token == '[')
        {
            token = Brak;
            return;
        }

        else if(token == '?')
        {
            token = Cond;
            return;
        }

        // directly return the character as token;
        else if(token == '~' || token == ';' || token == '{' || token == '}' || 
                token == '(' || token == ')' || token == ']' || token == ',' || token == ':') return;
    }
    return;
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

/* entrance of VM*/
int eval()
{
    long op, *tmp;
    while(1)
    {
        op = *pc++;
        if(op == IMM) ax = *pc++; // load immediate value to ax
        else if(op == LC) ax = *(char *)ax; // load character to ax, address in ax
        else if(op == LI) ax = *(long *)ax; // load integer to ax, address in ax
        else if(op == SC) ax = *(char *)*sp++ = ax; // save character to address, value in ax, address on stack
        else if(op == SI) *(long *)*sp++ = ax; // save integer to address, value in ax, address on stack
        // MOVE
        
        else if(op == PUSH) *--sp = ax; // push the value of ax onto the stack
        // PUSH
        
        else if(op == JMP) pc = (long *)*pc; // jump to the address
        // JUMP
        
        else if(op == JZ) pc = ax ? pc + 1: (long *) *pc; // jump if ax == 0
        else if(op == JNZ) pc = ax ? (long *) *pc : pc + 1; // jump if ax != 0
        // JZ/JNZ
        
        else if(op == CALL) 
        {
            *--sp = (long) (pc + 1); 
            pc = (long *) *pc; 
            // call subroutine
        }
        // else if(op == RET) pc = (int *) *sp ++;
        // CALL

        else if(op == ENT)
        {
            *--sp = (long) bp;
            bp = sp;
            sp = sp - *pc++;
            // make new stack frame
        }
        // ENT

        else if(op == ADJ) sp = sp + *pc++; // add esp, <size>
        // ADJ

        else if (op == LEV) 
        {
            sp = bp;
            bp = (long *) *sp++;
            pc = (long *) *sp++;
            // remove call frame and PC
        }
        // LEV (includes RET function)

        else if(op == LEA) ax = (long)(bp + *pc++); // load address for arguements
        // LEA

        else if(op == OR) ax = *sp++ | ax;
        else if(op == XOR) ax = *sp++ ^ ax;
        else if(op == AND) ax = *sp++ & ax;
        else if(op == EQ) ax = *sp++ == ax;
        else if(op == NE) ax = *sp++ != ax;
        else if(op == LT) ax = *sp++ < ax;
        else if(op == LE) ax = *sp++ <= ax;
        else if(op == GT) ax = *sp++ > ax;
        else if(op == GE) ax = *sp++ >= ax;
        else if(op == SHL) ax = *sp++ << ax;
        else if(op == SHR) ax = *sp++ >> ax;
        else if(op == ADD) ax = *sp++ + ax;
        else if(op == SUB) ax = *sp++ - ax;
        else if(op == MUL) ax = *sp++ * ax;
        else if(op == DIV) ax = *sp++ / ax;
        else if(op == MOD) ax = *sp++ % ax;

        else if(op == EXIT)
        {
            printf("exit(%ld)\n", *sp);
            return *sp;
        }
        else if(op == OPEN) ax = open((char *)sp[1], sp[0]);
        else if(op == CLOS) ax = close (*sp);
        else if(op == READ) ax = read(sp[2], (char *)sp[1], *sp);
        else if(op == PRTF)
        {
            tmp = sp + pc[1];
            ax = printf((char *)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]);
        }
        else if(op == MALC) ax = (long)malloc(*sp);
        else if(op == MSET) ax = (long)memset((char *)sp[2], sp[1], *sp);
        else if(op == MCMP) ax = memcmp((char *)sp[2], (char *)sp[1], *sp);
        else
        {
            printf("unknown instructuions: %ld\n", op);
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
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
    /*
    // test "10 + 20"
    i = 0;
    text[i++] = IMM;
    text[i++] = 10;
    text[i++] = PUSH;
    text[i++] = IMM;
    text[i++] = 20;
    text[i++] = ADD;
    text[i++] = PUSH;
    text[i++] = EXIT;
    pc = text;
    */

    // program();
    return eval();
}