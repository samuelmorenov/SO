#include "OperatingSystem.h"
#include "OperatingSystemBase.h"
#include "MMU.h"
#include "Processor.h"
#include "Buses.h"
#include "Heap.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>


// Functions prototypes
void OperatingSystem_PrepareDaemons();
void OperatingSystem_PCBInitialization(int, int, int, int, int);
void OperatingSystem_MoveToTheREADYState(int);
void OperatingSystem_Dispatch(int);
void OperatingSystem_RestoreContext(int);
void OperatingSystem_SaveContext(int);
void OperatingSystem_TerminateProcess();
int OperatingSystem_LongTermScheduler();
void OperatingSystem_PreemptRunningProcess();
int OperatingSystem_CreateProcess(int);
int OperatingSystem_ObtainMainMemory(int, int);
int OperatingSystem_ShortTermScheduler();
int OperatingSystem_ExtractFromReadyToRun();
void OperatingSystem_HandleException();
void OperatingSystem_HandleSystemCall();
void OperatingSystem_PrintReadyToRunQueue();
void OperatingSystem_Cambio_Estado(int ID, int anterior, char const *posterior);
void OperatingSystem_Transfer();
void OperatingSystem_HandleClockInterrupt();
void OperatingSystem_MoveToTheSleepingProcessesQueue(int PID);

// The process table
PCB processTable[PROCESSTABLEMAXSIZE];

// Address base for OS code in this version
int OS_address_base = PROCESSTABLEMAXSIZE * MAINMEMORYSECTIONSIZE;

// Identifier of the current executing process
int executingProcessID=NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Begin indes for daemons in programList
int baseDaemonsInProgramList; 

// Array that contains the identifiers of the READY processes
//Ejercicio 11 - Enunciado
//int readyToRunQueue[PROCESSTABLEMAXSIZE];
int readyToRunQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
//int numberOfReadyToRunProcesses=0;
int numberOfReadyToRunProcesses[NUMBEROFQUEUES] = { 0,0 };
char * queueNames[NUMBEROFQUEUES] = { "USER","DAEMONS" };


// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses=0;

//Ejercicio 10
char * statesNames [5]={"NEW","READY","EXECUTING","BLOCKED","EXIT"};

//Ejercicio 11 - 0
// Array that contains basic data abaut all deamons
// and all user programs specified in the command line
PROGRAMS_DATA *programList[PROGRAMSMAXNUMBER];

int executingProgressID = NOPROCESS;

//Ejercicio 4
int numberOfClockInterrupts = 0;

// TODO Ejercicio 5 Pega este código en el fichero indicado:
// In OperatingSystem.c Exercise 5-b of V2
// Heap with blocked processes sort by when to wakeup
int sleepingProcessesQueue[PROCESSTABLEMAXSIZE];
int numberOfSleepingProcesses=0;

// Initial set of tasks of the OS
//Ejercicio 14
void OperatingSystem_Initialize(int daemonsIndex) {
	
	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code

	// Obtain the memory requirements of the program
	int processSize=OperatingSystem_ObtainProgramSize(&programFile, "OperatingSystemCode");

	// Load Operating System Code
	OperatingSystem_LoadProgram(programFile, OS_address_base, processSize);
	
	// Process table initialization (all entries are free)
	for (i=0; i<PROCESSTABLEMAXSIZE;i++)
		processTable[i].busy=0;
	
	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base+1);
		
	// Create all system daemon processes
	OperatingSystem_PrepareDaemons(daemonsIndex);
	
	// Create all user processes from the information given in the command line
	//Ejercicio 14
	if(OperatingSystem_LongTermScheduler() == 0){
		OperatingSystem_TerminateProcess();
	}
	
	if (strcmp(programList[processTable[sipID].programListIndex]->executableName,"SystemIdleProcess")) {
		// Show message "ERROR: Missing SIP program!\n"
		ComputerSystem_DebugMessage(21,SHUTDOWN);
		exit(1);		
	}

	// At least, one user process has been created
	// Select the first process that is going to use the processor
	selectedProcess=OperatingSystem_ShortTermScheduler();

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);
}

// Daemon processes are system processes, that is, they work together with the OS.
// The System Idle Process uses the CPU whenever a user process is able to use it
void OperatingSystem_PrepareDaemons(int programListDaemonsBase) {
  
	// Include a entry for SystemIdleProcess at 0 position
	programList[0]=(PROGRAMS_DATA *) malloc(sizeof(PROGRAMS_DATA));

	programList[0]->executableName="SystemIdleProcess";
	programList[0]->arrivalTime=0;
	programList[0]->type=DAEMONPROGRAM; // daemon program

	sipID=INITIALPID%PROCESSTABLEMAXSIZE; // first PID for sipID

	// Prepare aditionals daemons here
	// index for aditionals daemons program in programList
	baseDaemonsInProgramList=programListDaemonsBase;

}

/////////////////////////////////////////////////////////////////////////////
/*
Modifica la funcion OperatingSystem_LongTermScheduler(), para que distinga el
caso de creacion de proceso con oxito y el error en caso de que lo hubiese, indicondolo
mediante la funcion ComputerSystem_DebugMessage(), utilizando el nomero de
mensaje 103, y la constante ERROR como valor para el segundo argumento de la misma
(seccion de interos). El mensaje debe tener el aspecto siguiente:
*/
/////////////////////////////////////////////////////////////////////////////
// The LTS is responsible of the admission of new processes in the system.
// Initially, it creates a process from each program specified in the 
// 			command lineand daemons programs
int OperatingSystem_LongTermScheduler() {
  
	int PID, i,
		numberOfSuccessfullyCreatedProcesses=0;
	
	for (i=0; programList[i]!=NULL && i<PROGRAMSMAXNUMBER; i++) {
		PID=OperatingSystem_CreateProcess(i);
		char *name = programList[i]->executableName;
		switch (PID) {
		case NOFREEENTRY:
			ComputerSystem_DebugMessage(103, INIT, name);
			break;

		case PROGRAMDOESNOTEXIST:
			ComputerSystem_DebugMessage(104, INIT, name, "--- it does not exist ---");
			break;

		case PROGRAMNOTVALID:
			ComputerSystem_DebugMessage(104, INIT, name, "--- invalid priority or size ---");
			break;
		case TOOBIGPROCESS:
			ComputerSystem_DebugMessage(105, INIT, name);
			break;

		default:
			numberOfSuccessfullyCreatedProcesses++;
			if (programList[i]->type == USERPROGRAM)
				numberOfNotTerminatedUserProcesses++;
			// Move process to the ready state
			OperatingSystem_MoveToTheREADYState(PID);
			break;
		}
	}

	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}

/////////////////////////////////////////////////////////////////////////////
/*
Modifica la funcion OperatingSystem_CreateProcess(), para que devuelva a la
funcion OperatingSystem_LongTermScheduler() el valor NOFREEENTRY cuando la
primera funcion fracasa al intentar conseguir una entrada libre en la tabla de procesos.
*/
/////////////////////////////////////////////////////////////////////////////
// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram) {
  
	int PID;
	int processSize;
	int loadingPhysicalAddress;
	int priority;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram=programList[indexOfExecutableProgram];

	// Obtain a process ID
	PID=OperatingSystem_ObtainAnEntryInTheProcessTable();
	if (PID == NOFREEENTRY)
		return PID;
	// Obtain the memory requirements of the program
	processSize=OperatingSystem_ObtainProgramSize(&programFile, executableProgram->executableName);	
	if (processSize == PROGRAMDOESNOTEXIST || processSize == PROGRAMNOTVALID)
		return processSize;
	// Obtain the priority for the process
	priority=OperatingSystem_ObtainPriority(programFile);
	if (priority == PROGRAMNOTVALID) 
		return priority;
	// Obtain enough memory space
 	loadingPhysicalAddress=OperatingSystem_ObtainMainMemory(processSize, PID);
	if (loadingPhysicalAddress == TOOBIGPROCESS)
		return loadingPhysicalAddress;
	// Load program in the allocated memory
	if(OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress, processSize) == TOOBIGPROCESS) return TOOBIGPROCESS;
	// PCB initialization
	OperatingSystem_PCBInitialization(PID, loadingPhysicalAddress, processSize, priority, indexOfExecutableProgram);
	// Show message "Process [PID] created from program [executableName]\n"
	ComputerSystem_DebugMessage(22,INIT,PID,executableProgram->executableName);

	return PID;
}


// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID) {

 	if (processSize>MAINMEMORYSECTIONSIZE)
		return TOOBIGPROCESS;
	
 	return PID*MAINMEMORYSECTIONSIZE;
}


// Assign initial values to all fields inside the PCB
//Ejercicio 11 - 1
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex) {

	processTable[PID].busy=1;
	processTable[PID].initialPhysicalAddress=initialPhysicalAddress;
	processTable[PID].processSize=processSize;
	processTable[PID].state=NEW;
	processTable[PID].priority=priority;
	processTable[PID].programListIndex=processPLIndex;
	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM) {
		//Ejericio 11

		processTable[PID].queueID=1;
		processTable[PID].copyOfPCRegister=initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister= ((unsigned int) 1) << EXECUTION_MODE_BIT;
	} 
	else {
		processTable[PID].queueID=0;
		processTable[PID].copyOfPCRegister=0;
		processTable[PID].copyOfPSWRegister=0;
	}
	//New process [SystemIdleProcess] moving to the [NEW] state
	ComputerSystem_DebugMessage(111, SYSPROC, programList[processTable[PID].programListIndex]->executableName);
}


// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID) {
	//Ejercicio 11 - 2
	if (Heap_add(PID, readyToRunQueue[processTable[PID].queueID],QUEUE_PRIORITY,&numberOfReadyToRunProcesses[processTable[PID].queueID],PROCESSTABLEMAXSIZE)>=0) {
		int anterior = processTable[PID].state;
		processTable[PID].state=READY;
		OperatingSystem_Cambio_Estado(PID, anterior, "READY");
	} 
	OperatingSystem_PrintReadyToRunQueue();
}


// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
//int OperatingSystem_ShortTermScheduler() {
//	
//	int selectedProcess;
//
//	selectedProcess=OperatingSystem_ExtractFromReadyToRun();
//	
//	return selectedProcess;
//}
//Ejercicio 11 - 3
int OperatingSystem_ShortTermScheduler() {
	int selectedProcess = NOPROCESS;
	int i;
	for (i = 0; i < NUMBEROFQUEUES && selectedProcess == NOPROCESS; i++){
		selectedProcess = OperatingSystem_ExtractFromReadyToRun(i);
	}
	return selectedProcess;
}


// Return PID of more priority process in the READY queue
// Ejercicio 11 - 4
int OperatingSystem_ExtractFromReadyToRun(int queueID){

	int selectedProcess=NOPROCESS;
	//selectedProcess=Heap_poll(readyToRunQueue,QUEUE_PRIORITY ,&numberOfReadyToRunProcesses);
	selectedProcess=Heap_poll(readyToRunQueue[queueID],QUEUE_PRIORITY ,&numberOfReadyToRunProcesses[queueID]);
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess; 
}


// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID) {

	// The process identified by PID becomes the current executing process
	executingProcessID=PID;
	// Change the process' state
	int anterior = processTable[PID].state;
	processTable[PID].state=EXECUTING;
	OperatingSystem_Cambio_Estado(executingProcessID, anterior, "EXECUTING");
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}


// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID) {
  
	// New values for the CPU registers are obtained from the PCB
	Processor_CopyInSystemStack(MAINMEMORYSIZE-1,processTable[PID].copyOfPCRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE-2,processTable[PID].copyOfPSWRegister);
	
	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);
}


// Function invoked when the executing process leaves the CPU 
// Ejercicio 12
void OperatingSystem_PreemptRunningProcess() {
	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state
	OperatingSystem_MoveToTheREADYState(executingProcessID);
	// The processor is not assigned until the OS selects another process
	executingProcessID=NOPROCESS;
}


// Save in the process' PCB essential values stored in hardware registers and the system stack
// Ejercicio 13
void OperatingSystem_SaveContext(int PID) {
	
	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-1);
	
	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-2);
}


// Exception management routine
void OperatingSystem_HandleException() {
  
	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	ComputerSystem_DebugMessage(23,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
	
	OperatingSystem_TerminateProcess();
}


// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess() {
  
	int selectedProcess;
  	int anterior = processTable[executingProcessID].state;
	processTable[executingProcessID].state=EXIT;
	OperatingSystem_Cambio_Estado(executingProcessID, anterior, "EXIT");
	
	if (programList[processTable[executingProcessID].programListIndex]->type==USERPROGRAM) 
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;
	
	if (numberOfNotTerminatedUserProcesses<=0) {
		// Simulation must finish 
		OperatingSystem_ReadyToShutdown();
	}
	// Select the next process to execute (sipID if no more user processes)
	selectedProcess=OperatingSystem_ShortTermScheduler();
	// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);
}

// System call management routine
void OperatingSystem_HandleSystemCall() {
  
	int systemCallID;

	// Register A contains the identifier of the issued system call
	systemCallID=Processor_GetRegisterA();
	
	switch (systemCallID) {
		case SYSCALL_PRINTEXECPID:
			// Show message: "Process [executingProcessID] has the processor assigned\n"
			ComputerSystem_DebugMessage(24,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			break;

		case SYSCALL_END:
			// Show message: "Process [executingProcessID] has requested to terminate\n"
			ComputerSystem_DebugMessage(25,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			OperatingSystem_TerminateProcess();
			break;

		case SYSCALL_YIELD:
			OperatingSystem_Transfer();
			break;
		//Todo Ejercicio 5
		case SYSCALL_SLEEP:
			OperatingSystem_MoveToTheSleepingProcessesQueue(executingProcessID);
			break;



	}
}
//Ejercicio 12
// Process [1 – progName1] will transfer the control of the processor to process [3 – progName2]
void OperatingSystem_Transfer(){
	int priority = processTable[executingProcessID].priority;
	//Localizar proceso para cambiar:
	int indexMasAlta = -1;
	int j = 0;
	int i = 0;
	for (j = 0; j < 2; j++) {
		for (i = 0; i < numberOfReadyToRunProcesses[j]; i++) {
			int identificador = readyToRunQueue[j][i];
			int prioridadAux = processTable[identificador].priority;
			if(prioridadAux == priority){
				indexMasAlta = identificador;
			}
		}
	}
	if(indexMasAlta == -1){
		return;
	}
	//Imprimir el cambio:
	char *nameMasAlta = programList[processTable[indexMasAlta].programListIndex]->executableName;
	char *name = programList[processTable[executingProcessID].programListIndex]->executableName;
	ComputerSystem_DebugMessage(115,SHORTTERMSCHEDULE, executingProcessID, name, indexMasAlta, nameMasAlta);
	//Quitar el procesador al proceso actual:
	OperatingSystem_PreemptRunningProcess();
	//Dar el procesador al siguiente proceso:
	OperatingSystem_Dispatch(indexMasAlta);
}
	
//	Implement interrupt logic calling appropriate interrupt handle
void OperatingSystem_InterruptLogic(int entryPoint){
	switch (entryPoint){
		case SYSCALL_BIT: // SYSCALL_BIT=2
			OperatingSystem_HandleSystemCall();
			break;
		case EXCEPTION_BIT: // EXCEPTION_BIT=6
			OperatingSystem_HandleException();
			break;
		case CLOCKINT_BIT: // CLOCKINT_BIT=9
			OperatingSystem_HandleClockInterrupt();
			break;


	}

}

/*
Ejerccio 9
* /
void OperatingSystem_PrintReadyToRunQueue(){
	ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE);

	//int a[17];
	//size_t n = sizeof(readyToRunQueue)/sizeof(readyToRunQueue[0]);

	int i = 0;
	for (i=0; i<numberOfReadyToRunProcesses; i++) {
		int identificador = readyToRunQueue[i];
		int prioridad = processTable[identificador].priority;

		if(i == 0){
			ComputerSystem_DebugMessage(107, SHORTTERMSCHEDULE, identificador, prioridad);
		}
		else{
			ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, identificador, prioridad);
		}
		
	}
	ComputerSystem_DebugMessage(109, SHORTTERMSCHEDULE);
	
}
*/
//Ejercicio 11 - 5
 void OperatingSystem_PrintReadyToRunQueue() {
	ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE);

	int i = 0;
	int j = 0;
	for (j = 0; j < 2; j++) {
		ComputerSystem_DebugMessage(112, SHORTTERMSCHEDULE, queueNames[j]);
		for (i = 0; i < numberOfReadyToRunProcesses[j]; i++) {
			int identificador = readyToRunQueue[j][i];
			int prioridad = processTable[identificador].priority;
			if (i == 0) {
				ComputerSystem_DebugMessage(113, SHORTTERMSCHEDULE, identificador, prioridad);
			} else {
				ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, identificador, prioridad);
			}
		}
		ComputerSystem_DebugMessage(109, SHORTTERMSCHEDULE);
	}


}

void OperatingSystem_Cambio_Estado(int ID, int anterior, char const *posterior){
	ComputerSystem_DebugMessage(110, SYSPROC,
	ID,
	programList[processTable[ID].programListIndex]->executableName,
	statesNames[anterior],
	posterior);
	
}

// In OperatingSystem.c Exercise 2-b of V2
// Ejercicio 2 - Pega el código de debajo en el fichero indicado en el
// comentario correspondiente y añade la función prototipo donde sea necesario.
// Ejercicio 4 - Modifica la rutina OperatingSystem_HandleClockInterrupt() para
// que cuente el número total de interrupciones de reloj ocurridas
// (en la variable numberOfClockInterrupts) y muestre un mensaje en pantalla
// con el aspecto siguiente (número de mensaje 120, sección INTERRUPT, y color Cyan).
// Para el tiempo, usad la función OperatingSystem_ShowTime(INTERRUPT)

void OperatingSystem_HandleClockInterrupt(){
	ComputerSystem_DebugMessage(120, INTERRUPT, numberOfClockInterrupts);
	numberOfClockInterrupts = numberOfClockInterrupts + 1;
	return;
}

void OperatingSystem_MoveToTheSleepingProcessesQueue(int PID) {
	//TODO Ejercicio 5
	processTable[PID].whenToWakeUp = numberOfClockInterrupts + 1;


//	if (
//			Heap_add(PID, sleepingProcessesQueue[processTable[PID].queueID],QUEUE_PRIORITY,&numberOfReadyToRunProcesses[processTable[PID].queueID],PROCESSTABLEMAXSIZE)
//			>=
//			0
//			) {
//		int anterior = processTable[PID].state;
//		processTable[PID].state=READY;
//		OperatingSystem_Cambio_Estado(PID, anterior, "READY");
//	}

	Test("Movido, whenToWakeUp = ", processTable[PID].whenToWakeUp);
}


void Test(char const *cadena, int numero){
	#define ANSI_COLOR_BLUE "\x1b[34m"
	#define ANSI_COLOR_RESET "\x1b[0m"
	char chr;
	printf(ANSI_COLOR_BLUE);
	printf("\t%s %d >>", cadena, numero);
	printf(ANSI_COLOR_RESET);
	scanf("%c",&chr);
}
