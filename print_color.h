#ifndef PRINT_COLOR_INCLUDED
#define PRINT_COLOR_H_INCLUDED
#include <stdio.h> 

void blue() {
    printf("\033[1;34m");
}

void green() {
    printf("\033[1;32m");
}

void magenta() {
    printf("\033[1;35m");
}

void reset() {
    printf("\033[0m");
}
#endif

