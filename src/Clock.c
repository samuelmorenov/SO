#include "Clock.h"
#include "Processor.h"
#include "OperatingSystem.h"
// #include "ComputerSystem.h"

int tics=0;
#define INTERVALBETWEENINTERRUPS 5

void Clock_Update() {

	tics++;
	OperatingSystem_InterruptLogic(CLOCKINT_BIT);
    // ComputerSystem_DebugMessage(97,CLOCK,tics);
}


int Clock_GetTime() {

	return tics;
}
