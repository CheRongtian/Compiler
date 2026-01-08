#include "global.h"

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