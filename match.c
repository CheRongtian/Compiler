#include <stdio.h>
#include <stdlib.h>
#include "match.h"
#include "next.h"

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