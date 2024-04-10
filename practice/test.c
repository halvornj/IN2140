#include <stdio.h>

int main() {
    char nums[] = {16,24,255,0,63,64};

    printf("1624: %d",(nums[0]>>2| nums[1]));
}