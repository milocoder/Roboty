#include <iostream>
#include <wiringPi.h>
#include <wiringSerial.h>

using namespace std;

int main(){
	int fd;
	if((fd=serialOpen("/dev/serial0",9600))<0){
		printf("Unable to open serial\n");
		return 1;
	}
	
	for(;;){
		putchar(serialGetchar(fd));
		fflush(stdout);
	}
	wiringPiSetup();
	pinMode(1, OUTPUT);
	digitalWrite(1, HIGH);
	
}
