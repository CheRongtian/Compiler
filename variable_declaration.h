#ifndef VARIABLE_DECLARATION_H
#define VARIABLE_DECLARATION_H

extern long basetype;
extern long expr_type;
extern long *text;

// types of variable/function
enum {CHAR, INT, PTR};

void global_declaration();
void enum_declaration();
#endif