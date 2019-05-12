#ifndef OPERATINGSYSTEM_H
#define OPERATINGSYSTEM_H

#include "ComputerSystem.h"
#include <stdio.h>


#define SUCCESS 1
#define PROGRAMDOESNOTEXIST -1
#define PROGRAMNOTVALID -2

#define MAXLINELENGTH 150

#define PROCESSTABLEMAXSIZE 4

#define INITIALPID 3

// In this version, every process occupies a 60 positions main memory chunk 
// so we can use 60 positions for OS code and the system stack
#define MAINMEMORYSECTIONSIZE (MAINMEMORYSIZE / (PROCESSTABLEMAXSIZE+1))

#define NOFREEENTRY -3
#define TOOBIGPROCESS -4
#define MEMORYFULL -5  //Ejercicio V4.6

#define NOPROCESS -1

//Ejercicio 5 Define SLEEPINGQUEUE en OperatingSystem.h para que compile el código que depende de que se hayan definido las estructuras de la cola de procesos bloqueados.
#define SLEEPINGQUEUE

// Contains the possible type of programs
enum ProgramTypes { USERPROGRAM, DAEMONPROGRAM }; 

// Enumerated type containing all the possible process states
enum ProcessStates { NEW, READY, EXECUTING, BLOCKED, EXIT};

// Enumerated type containing the list of system calls and their numeric identifiers
//Ejercicio 12
//Ejercicio 5 Añade una nueva llamada al sistema SYSCALL_SLEEP=7 que bloqueará al proceso en ejecución (es decir, se tendrá que mover al estado BLOCKED) y lo insertará por orden creciente del campo whenToWakeUp en la sleepingProcessesQueue.
enum SystemCallIdentifiers { SYSCALL_END=3, SYSCALL_PRINTEXECPID=5, SYSCALL_YIELD=4, SYSCALL_SLEEP=7, SYSCALL_IO=1};

//Ejercicio 11 - 0/Enunciado
#define NUMBEROFQUEUES 2
enum ReadyToRunProcessQueues { USERPROCESSQUEUE, DAEMONSQUEUE };

//Ejercicio V4.5
// Partitions configuration file name definition
#define MEMCONFIG "MemConfig"  // in OperatingSystem.h

// A PCB contains all of the information about a process that is needed by the OS
typedef struct {
	int busy;
	int initialPhysicalAddress;
	int processSize;
	int state;
	int priority;
	int copyOfPCRegister;
	unsigned int copyOfPSWRegister;
	int programListIndex;
	int queueID; //Ejercicio 11 - Enunciado
	int whenToWakeUp; // Ejercicio 5 Añade a la struct PCB un campo adicional:
} PCB;

// These "extern" declaration enables other source code files to gain access
// to the variable listed
extern PCB processTable[PROCESSTABLEMAXSIZE];
extern int OS_address_base;
extern int sipID;

// Functions prototypes
void OperatingSystem_Initialize();
void OperatingSystem_InterruptLogic(int);
int OperatingSystem_GetExecutingProcessID();


#endif
