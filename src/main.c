#include <stdio.h>

#define INCHES_PER_POUND 166 // directive declaration

void one(); // function declaration
void two();

int main () {
    printf("Hello world!. I wrote my firs program ic C!\n");

    one();
    two();


    return 0;
}

void one() {
    int integer = 2;
    float floatingPoint = 5.0256f;
    printf("An integer number is: %d, a floating point number is: %.2f\n", integer, floatingPoint); // 0.nf - round a number to n chars
}

void two() {
    int height, length, width, volume, weight;

    printf("Enter height of box: ");
    scanf("%d", &height);
    printf("Enter length of box: ");
    scanf("%d", &length);
    printf("Enter width of box: ");
    scanf("%d", &width);

    volume = height * length * width;
    weight = (volume + INCHES_PER_POUND - 1) / INCHES_PER_POUND;

    printf("Volume (cubic inches): %d\n", volume);
    printf("Dimensional weight (pounds): %d\n", weight);
}