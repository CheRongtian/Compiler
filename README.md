# Trying to make a C compiler using C

Based on the instruction from: 
https://github.com/lotabout/write-a-C-interpreter/blob/master/README.md

But modify some by myself:
- Change from ``` int ``` to ``` long ``` to fit my 64-bit Mac Machine
- Use ```.h``` file and ```CMake``` to separate each module(I like ```CMake``` ✌︎( ᐛ )✌︎)

## How to compiler + test running:
```bash
cd build
make // compiler
./compiler ../test.c
```

But do not finish, so, now cannot do this function :)

## 💡Right now: (2025.12.02)

```
next.c produces Add / Sub / Mul / Div (enum values).
expr() expects '+' '-' '*' '/' (characters).

They never match.
```
- So, still cannot work hhhhhhhh