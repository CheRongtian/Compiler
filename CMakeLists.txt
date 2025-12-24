cmake_minimum_required(VERSION 3.10)
project(Compiler C)

add_executable(compiler 
               compiler.c 
               eval.c 
               next.c 
               parse.c 
               program.c 
               variable_declaration.c 
               function_declaration.c 
               statement.c 
               expression.c 
               match.c)