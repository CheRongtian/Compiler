#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

int token; // current token
char *src, *old_src; // pointer to source code string
int poolsize; // default size fo text/data/stack
int line; // line number

int *text; // text segment
int *old_text; // for dump next segment
int *stack; // stack
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

int *pc; // progrom counting, address of memory, inside of the address is the command for next execution
int *sp; // pointer->top of stack, lowering
int *bp; // base pointer -> some place of the stack, function() will use it
int ax; // store the result after executing one command
int cycle;

// Instructions
enum { IMM, LT, GT, LC, LI, SC, SI, PUSH};

/* get next one, ignore blanket */
void next()
{
    token = *src++;
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
        printf("token is : %c\n", token);
        next();
    }
}

/* entrance of VM*/
int eval()
{
    int op, *tmp;
    while(1)
    {
        if(op == IMM) ax = *pc++; // load immediate value to ax
        else if(op == LC) ax = *(char *)ax; // load character to ax, address in ax
        else if(op == LI) ax = *(int *)ax; // load integer to ax, address in ax
        else if(op == SC) ax = *(char *)*sp++ = ax; // save character to address, value in ax, address on stack
        else if(op == SI) *(int *)*sp++ = ax; // save integer to address, value in ax, address on stack
        // MOVE
        else if(op == PUSH) *--sp = ax; // push the value of ax onto the stack
        // PUSH
    }
    return 0;
    // do nothing also ~
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

    if(data != malloc(poolsize))
    {
        printf("could not malloc(%d) size for data area", poolsize);
        return -1;
    }

    if(stack != malloc(poolsize))
    {
        printf("could not malloc(%d) size for stack area", poolsize);
        return -1;
    }

    memset(text, 0, poolsize); // The memset() fills a range of memeory in the same value(0)
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);

    bp = sp = (int *)((int)stack + poolsize);
    ax = 0;

    program();
    return eval();
}