#include "mbed.h"
#include <cstdio>
DigitalOut TRIG(D13);
AnalogIn ECHO(A0);
Timer timerMeasure;
Ticker secTicker;
bool performMeasurement = false;

void tickerFunc() {
    performMeasurement = true;
}

void measure() {
    bool timerOn = false;

    TRIG = 1;
    wait_us(10);
    TRIG = 0;

    while(1) {
        if(ECHO.read() > 0.2 && !timerOn) {
            timerMeasure.start();
            timerOn = true;
        }
        if(timerOn) {
            if(ECHO.read() < 0.2) {
                timerMeasure.stop();
                double pulseDuration = timerMeasure.elapsed_time().count(); // in microseconde
                double pulseDurationSeconde = pulseDuration / 1000000; // in seconde
                double distanceCentimeter = pulseDurationSeconde * 343 * 100 / 2;
                // tijd * snelheid * 100 (van m naar cm) / 2 (pulse gaat heen en weer, dus afstand tot object is distance / 2)
                printf("Afstand is: %.2f cm\n", distanceCentimeter);
                break;
            }
        }
    }
    timerMeasure.reset();
    performMeasurement = false;
}

int main() {
    secTicker.attach(&tickerFunc, 1s);

    while(1) {
        if(performMeasurement) {
            measure();
        }
    }
}