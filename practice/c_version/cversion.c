#include <stdio.h>

int main(int argc, char** argv){

    #define SQUARE(x) x*x

    printf("%d\n", SQUARE(5));
    printf("%i\n", SQUARE(3+1));

    //single-line declaration and initialization allows self reference!
    //reference is after init
    int size = sizeof(size);
    printf("size: %i\n", size);

    // Indicate successful execution
    return 0;
    }