# Trying to make a C compiler using C

Based on the instruction from: 
https://github.com/lotabout/write-a-C-interpreter/blob/master/README.md

But modify some by myself:
- Change from ``` int ``` to ``` long ``` to fit my 64-bit Mac Machine
- Use ```.h``` file and ```CMake``` ( ✌︎( ᐛ )✌︎)

## Final Structure
```
reorganized version/
├── global.h
├── main.c
├── lexer.c
├── parser.c
└── vm.c
```

- **global.h** (The Registry): Stores all shared definitions (Token IDs, instruction codes, register variable declarations). Any file that needs to communicate with others must include this.

- **main.c** (The Assembly Plant): Responsible for allocating memory and reading source files. It first calls the parser to work, and then triggers the VM to run.

- **lexer.c** (The Cutter): Solely manages ```next()```. It chops the long source string into individual Tokens (e.g., ```int```, ```main```, ```123```).

- **parser.c** (The Translator): Solely manages ```program()```, ```expression()```, etc. It observes the Tokens and writes instructions (assembly) into the ```text``` array.

- **vm.c** (The CPU): Solely manages ```eval()```. It reads the instructions from the ```text``` array and executes them one by one.


## How to use this?

How to compile + test running:
```bash
mkdir build
cd build
cmake ..
make
./compiler ../test.c
```
Then you can get:

```
xxx build % make                
[100%] Built target compiler
xxx build % ./compiler ../test.c
fibonacci(0) = 1
fibonacci(1) = 1
fibonacci(2) = 2
fibonacci(3) = 3
fibonacci(4) = 5
fibonacci(5) = 8
fibonacci(6) = 13
fibonacci(7) = 21
fibonacci(8) = 34
fibonacci(9) = 55
fibonacci(10) = 89
exit(0)
```

## Reorganize Git Command

```bash
git checkout -b final-version
git add "folder_name"
git commit -m "xxxx"
git push -u original final-version
```