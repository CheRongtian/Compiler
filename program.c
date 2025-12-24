#include "program.h"
#include "next.h"
#include "variable_declaration.h"

/* entrance of grammar analysis, analyzing the whole c-file*/
void program()
{
    next(); // get next token
    while(token > 0) global_declaration();
}