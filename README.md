# Trying to make a C compiler using C

Based on the instruction from: 
https://github.com/lotabout/write-a-C-interpreter/blob/master/README.md

But modify some by myself:
- Change from ``` int ``` to ``` long ``` to fit my 64-bit Mac Machine
- Use ```.h``` file and ```CMake``` to separate each module( ✌︎( ᐛ )✌︎)

## How to compiler + test running:
```bash
cd build
make // compiler
./compiler ../test.c
```
Then you can get:

```bash
xxx build % make                
[100%] Built target compiler
xxx build % ./compiler ../test.c
fibonacci( 0) = 1
fibonacci( 1) = 1
fibonacci( 2) = 2
fibonacci( 3) = 3
fibonacci( 4) = 5
fibonacci( 5) = 8
fibonacci( 6) = 13
fibonacci( 7) = 21
fibonacci( 8) = 34
fibonacci( 9) = 55
fibonacci(10) = 89
exit(0)
```