# include <stdio.h>
// test file
int fibonacci(int i) {
    if (i <= 1) {
        return 1;
    }
    return fibonacci(i-1) + fibonacci(i-2);
}

int main()
{
    int i;
    i = 0;
    while (i <= 10) {
        printf("fibonacci(%d) = %d\n", i, fibonacci(i));
        i = i + 1;
    }
    exit(0);
    //return 0;
}