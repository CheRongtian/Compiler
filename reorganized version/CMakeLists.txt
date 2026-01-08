cmake_minimum_required(VERSION 3.10)
project(Compiler C)

add_executable(compiler
               global
               lexer.c
               main.c
               parser.c
               vm.c)