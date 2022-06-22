#include "mbed.h"
#include <cstdio>
#include "FileHandle.h"

DigitalIn swTop(D8);
DigitalIn swBottom(D9);
DigitalOut motorA(D4);
DigitalOut motorB(D5);
void hefMechanisme();

int main()
{
    motorA = 0;
    motorB = 0;

    wait_us(2000000);
    hefMechanisme();
}

void hefMechanisme()
{
    motorA = 1; //          motorA aan om de ene richting op te draaien
    while(!swTop)
    {}
    motorA = 0; //          motorA uit om te stoppen met draaien
    wait_us(10000000); //   wacht 10 seconde om stapel papier te laten schuiven
    motorB = 1; //          motorB aan om de andere richting op te draaien
    while(!swBottom)
    {}
    motorB = 0; //          motorB uit
}

