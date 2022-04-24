#include "mbed.h"
#include <cstdio>
DigitalOut TRIG(D13);
AnalogIn ECHO(A0);
Timer timer;

int main() {
    bool timerOn = false;
    wait_us(2500000);

    TRIG = 1;
    wait_us(10);
    TRIG = 0;

    while(1) {
        if(ECHO.read() > 0.2 && !timerOn) {
            timer.start();
            timerOn = true;
        }
        if(timerOn) {
            if(ECHO.read() < 0.2) {
                timer.stop();
                double pulseDuration = timer.elapsed_time().count(); // in microseconde
                double pulseDurationSeconde = pulseDuration / 1000000; // in seconde
                double distanceCentimeter = pulseDurationSeconde * 343 * 100 / 2;
                // tijd * snelheid * 100 (van m naar cm) / 2 (pulse gaat heen en weer, dus afstand tot object is distance / 2)
                printf("Afstand is: %.2f cm\n", distanceCentimeter);
                break;
            }
        }
    }
}