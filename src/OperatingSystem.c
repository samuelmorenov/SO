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
void OperatingSystem_HandleException(int excepcion);
void OperatingSystem_HandleSystemCall();
void OperatingSystem_PrintReadyToRunQueue();
void OperatingSystem_Print_Cambio_Estado(int ID, int anterior,
		char const *posterior);
void OperatingSystem_TransferWithEcualPriority();
void OperatingSystem_HandleClockInterrupt();
void OperatingSystem_MoveToTheSleepingProcessesQueue();
void OperatingSystem_WakeUpProcesses();
int OperatingSystem_ExtractFromSleepingQueue(int queueID);
void OperatingSystem_Dormir_Proceso_Actual();
int OperatingSystem_GetWhenToWakeUp();
int OperatingSystem_CambiarProcesoAlMasPrioritario();
int OperatingSystem_BestPartition(int processSize);
void Test(char const *cadena, int numero);

// The process table
PCB processTable[PROCESSTABLEMAXSIZE];

// Address base for OS code in this version
int OS_address_base = PROCESSTABLEMAXSIZE * MAINMEMORYSECTIONSIZE;

// Identifier of the current executing process
int executingProcessID = NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Begin indes for daemons in programList
int baseDaemonsInProgramList;

// Array that contains the identifiers of the READY processes
//Ejercicio 11 - Enunciado
//int readyToRunQueue[PROCESSTABLEMAXSIZE];
int readyToRunQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
//int numberOfReadyToRunProcesses=0;
int numberOfReadyToRunProcesses[NUMBEROFQUEUES] = { 0, 0 };
char * queueNames[NUMBEROFQUEUES] = { "USER", "DAEMONS" };

// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses = 0;

//Ejercicio 10
char * statesNames[5] = { "NEW", "READY", "EXECUTING", "BLOCKED", "EXIT" };

//Ejercicio 11 - 0
// Array that contains basic data abaut all deamons
// and all user programs specified in the command line
PROGRAMS_DATA *programList[PROGRAMSMAXNUMBER];

int executingProgressID = NOPROCESS;

//Ejercicio 4
int numberOfClockInterrupts = 0;

// Ejercicio 5 Pega este código en el fichero indicado:
// In OperatingSystem.c Exercise 5-b of V2
// Heap with blocked processes sort by when to wakeup
int sleepingProcessesQueue[PROCESSTABLEMAXSIZE];
int numberOfSleepingProcesses = 0;

//Ejercicio V5.2
int IOWaitingProcessesQueue[PROCESSTABLEMAXSIZE];
int numberOfIOWaitingProcesses = 0;

// Initial set of tasks of the OS
/**
 * Inicializa el sistema operativo
 * Modificado: ejercicio V4.5
 */
void OperatingSystem_Initialize(int daemonsIndex) {

	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code

	//Ejercicio V4.5
	OperatingSystem_InitializePartitionTable();

	// Obtain the memory requirements of the program
	int processSize = OperatingSystem_ObtainProgramSize(&programFile,
			"OperatingSystemCode");

	// Load Operating System Code
	OperatingSystem_LoadProgram(programFile, OS_address_base, processSize);

	// Process table initialization (all entries are free)
	for (i = 0; i < PROCESSTABLEMAXSIZE; i++)
		processTable[i].busy = 0;

	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base + 1);

	// Create all system daemon processes
	OperatingSystem_PrepareDaemons(daemonsIndex);

	//Ejercicio V3.0
	ComputerSystem_FillInArrivalTimeQueue();
	OperatingSystem_PrintStatus();

	// Create all user processes from the information given in the command line
	//Ejercicio 14
	if (OperatingSystem_LongTermScheduler() == 0) {
		OperatingSystem_TerminateProcess();
	}

	if (strcmp(
			programList[processTable[sipID].programListIndex]->executableName,
			"SystemIdleProcess")) {
		// Show message "ERROR: Missing SIP program!\n"
		OperatingSystem_ShowTime(SHUTDOWN);
		ComputerSystem_DebugMessage(21, SHUTDOWN);
		exit(1);
	}

	// At least, one user process has been created
	// Select the first process that is going to use the processor
	selectedProcess = OperatingSystem_ShortTermScheduler();

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);

}

// Daemon processes are system processes, that is, they work together with the OS.
// The System Idle Process uses the CPU whenever a user process is able to use it
/**
 * Carga el proceso Daemons
 * Modificado: NO
 */
void OperatingSystem_PrepareDaemons(int programListDaemonsBase) {

	// Include a entry for SystemIdleProcess at 0 position
	programList[0] = (PROGRAMS_DATA *) malloc(sizeof(PROGRAMS_DATA));

	programList[0]->executableName = "SystemIdleProcess";
	programList[0]->arrivalTime = 0;
	programList[0]->type = DAEMONPROGRAM; // daemon program

	sipID = INITIALPID % PROCESSTABLEMAXSIZE; // first PID for sipID

	// Prepare aditionals daemons here
	// index for aditionals daemons program in programList
	baseDaemonsInProgramList = programListDaemonsBase;

}

// The LTS is responsible of the admission of new processes in the system.
// Initially, it creates a process from each program specified in the 
// 			command lineand daemons programs
/**
 * Planificador a largo plazo
 * Crea un proceso para cada programa. Da errores en caso de no poder
 * Modificado: V2.7
 */
int OperatingSystem_LongTermScheduler() {

	/*
	 * V3.2 Modifica tu planificador a largo plazo para que se intenten
	 * crear los procesos correspondientes mientras llamadas sucesivas
	 * a OperatingSystem_IsThereANewProgram devuelvan 1, indicando
	 * que hay programas que han llegado al sistema hasta el momento actual.
	 * Puedes mirar cómo se utiliza la función Heap_poll() en otras colas.
	 */

	int PID, i, numberOfSuccessfullyCreatedProcesses = 0;

	for (i = 0;
			programList[i] != NULL && i < PROGRAMSMAXNUMBER
					&& OperatingSystem_IsThereANewProgram() == 1; i++) {

		PID = OperatingSystem_CreateProcess(i);
		char *name = programList[i]->executableName;
		OperatingSystem_ShowTime(INIT);
		switch (PID) {
		case NOFREEENTRY:
			ComputerSystem_DebugMessage(103, INIT, name);
			break;

		case PROGRAMDOESNOTEXIST:
			ComputerSystem_DebugMessage(104, INIT, name,
					"--- it does not exist ---");
			break;

		case PROGRAMNOTVALID:
			ComputerSystem_DebugMessage(104, INIT, name,
					"--- invalid priority or size ---");
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
			OperatingSystem_PrintStatus(); //Ejercicio V2.7
			break;
		}
	}

	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}

// This function creates a process from an executable program
/**
 * Crea un proceso para un programa especificado en indexOfExecutableProgram
 * Modificado: V1.11
 */
int OperatingSystem_CreateProcess(int indexOfExecutableProgram) {

	int PID;
	int processSize;
	int loadingPhysicalAddress;
	int priority;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram = programList[indexOfExecutableProgram];

	// Obtain a process ID
	PID = OperatingSystem_ObtainAnEntryInTheProcessTable();
	if (PID == NOFREEENTRY)
		return PID;
	// Obtain the memory requirements of the program
	processSize = OperatingSystem_ObtainProgramSize(&programFile,
			executableProgram->executableName);
	if (processSize == PROGRAMDOESNOTEXIST || processSize == PROGRAMNOTVALID)
		return processSize;
	// Obtain the priority for the process
	priority = OperatingSystem_ObtainPriority(programFile);
	if (priority == PROGRAMNOTVALID)
		return priority;

	/////////////////////////////////////
	//TODO: Ejercicio 4.6

	// Obtain enough memory space
	int partition = OperatingSystem_ObtainMainMemory(processSize, PID);
	if (partition == TOOBIGPROCESS || partition == MEMORYFULL) {
		return partition;
	}
	loadingPhysicalAddress = partitionsTable[partition].initAddress;
	int partitionSize = partitionsTable[partition].size;
	// Load program in the allocated memory
	if (OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress,
			processSize) == TOOBIGPROCESS)
		return TOOBIGPROCESS;
	// PCB initialization
	OperatingSystem_PCBInitialization(PID, loadingPhysicalAddress, processSize,
			priority, indexOfExecutableProgram);
	// Show message "Process [PID] created from program [executableName]\n"
	OperatingSystem_ShowTime(INIT);
	ComputerSystem_DebugMessage(22, INIT, PID,
			executableProgram->executableName);

	OperatingSystem_ShowTime(SYSMEM);
	//ComputerSystem_DebugMessage(22, SYSMEM, partition, loadingPhysicalAddress, partitionSize, PID, executableProgram->executableName); //TODO Ejercicio 4.6

	return PID;
}

// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
/**
 * Asignacion de trozos de memoria
 * Modificado: 4.6
 */
int OperatingSystem_ObtainMainMemory(int processSize, int PID) {
	/*
	 * Modifica la función OperatingSystem_ObtainMainMemory()para que elija una par-tición
	 * para el proceso siguiendo la política (estrategia) del mejor ajuste; si hubiese
	 * varias, se debe elegir la que empiece en una dirección de memoria más baja
	 */
	if (processSize > MAINMEMORYSECTIONSIZE)
		return TOOBIGPROCESS;

	int bestPartition = OperatingSystem_BestPartition(processSize);

	if (!(bestPartition == TOOBIGPROCESS || bestPartition == MEMORYFULL)) {

		partitionsTable[bestPartition].occupied = 1;
		partitionsTable[bestPartition].PID = PID;

		OperatingSystem_ShowTime(SYSMEM);
		ComputerSystem_DebugMessage(142, SYSMEM, PID,
				programList[processTable[PID].programListIndex]->executableName,
				bestPartition);
	} else {
		//[1] ERROR:  A  process  could  not  be  created  from  program  [acceptableSizeExample]  because an appropriate partition is not available
		OperatingSystem_ShowTime(ERROR);
		ComputerSystem_DebugMessage(144, ERROR,
				programList[processTable[PID].programListIndex]->executableName);
	}

	return bestPartition;

}

int OperatingSystem_BestPartition(int processSize) {
	int currentPartition = 0;

	int bestPartition = -1;
	int bestSize = -1;

	int noEsDemasiadoGrande = 0;
	int algunaParticionLibre = 0;

	for (currentPartition = 0; currentPartition < PARTITIONTABLEMAXSIZE;
			currentPartition++) {
		int currentSize = partitionsTable[currentPartition].size;

		if (processSize - currentSize > -1) {
			noEsDemasiadoGrande = 1;

			if (partitionsTable[currentPartition].occupied == 0) {
				algunaParticionLibre = 1;

				if (currentSize < bestSize || bestSize == -1) {
					bestPartition = currentPartition;
					bestSize = currentSize;
				}
			}
		}

	}
	if (noEsDemasiadoGrande == 0) {
		return TOOBIGPROCESS;
	}
	if (algunaParticionLibre == 0) {
		return MEMORYFULL;
	}
	return bestPartition;
}

// Assign initial values to all fields inside the PCB
/**
 * Asigna los valores iniciales a un proceso PID
 * Modificado: V1.11
 */
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress,
		int processSize, int priority, int processPLIndex) {

	processTable[PID].busy = 1;
	processTable[PID].initialPhysicalAddress = initialPhysicalAddress;
	processTable[PID].processSize = processSize;
	processTable[PID].state = NEW;
	processTable[PID].priority = priority;
	processTable[PID].programListIndex = processPLIndex;
// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM) {
		//Ejericio 11

		processTable[PID].queueID = 1;
		processTable[PID].copyOfPCRegister = initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister = ((unsigned int) 1)
				<< EXECUTION_MODE_BIT;
	} else {
		processTable[PID].queueID = 0;
		processTable[PID].copyOfPCRegister = 0;
		processTable[PID].copyOfPSWRegister = 0;
	}
//New process [SystemIdleProcess] moving to the [NEW] state
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(111, SYSPROC,
			programList[processTable[PID].programListIndex]->executableName);
}

// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
/**
 * Añade el proceso PID a la cola de ready
 * Cambia el estado del proceso PID a ready
 * Imprime la lista de procesos ready
 * Modificado: V1.11
 */
void OperatingSystem_MoveToTheREADYState(int PID) {
	if (Heap_add(PID, readyToRunQueue[processTable[PID].queueID],
	QUEUE_PRIORITY, &numberOfReadyToRunProcesses[processTable[PID].queueID],
	PROCESSTABLEMAXSIZE) >= 0) {
		int anterior = processTable[PID].state;
		processTable[PID].state = READY;
		OperatingSystem_Print_Cambio_Estado(PID, anterior, "READY");
	}
//OperatingSystem_PrintReadyToRunQueue(); //Comentado en el ejercicio V2.8
}

// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
/**
 * Planificador a corto plazo
 * Busca el proceso de entre todas las colas ready con mas priodirdad
 * Modificado: V1.11
 */
int OperatingSystem_ShortTermScheduler() {
	int selectedProcess = NOPROCESS;
	int i;
	for (i = 0; i < NUMBEROFQUEUES && selectedProcess == NOPROCESS; i++) {
		selectedProcess = OperatingSystem_ExtractFromReadyToRun(i);
	}
	return selectedProcess;
}

// Return PID of more priority process in the READY queue
/**
 * Devuelve el PID con mas prioridad de la cola de readys que le pasas por queueID
 * Modificado: V1.11
 */
int OperatingSystem_ExtractFromReadyToRun(int queueID) {
	int selectedProcess = NOPROCESS;
//selectedProcess=Heap_poll(readyToRunQueue,QUEUE_PRIORITY ,&numberOfReadyToRunProcesses);
	selectedProcess = Heap_poll(readyToRunQueue[queueID], QUEUE_PRIORITY,
			&numberOfReadyToRunProcesses[queueID]);
// Return most priority process or NOPROCESS if empty queue
	return selectedProcess;
}

// Function that assigns the processor to a process
/**
 * Dado un PID, lo pone en ejecutando
 * Modificado: (Escribir que se ha modificado)
 */
void OperatingSystem_Dispatch(int PID) {

// The process identified by PID becomes the current executing process
	executingProcessID = PID;
// Change the process' state
	int anterior = processTable[PID].state;
	processTable[PID].state = EXECUTING;
	OperatingSystem_Print_Cambio_Estado(executingProcessID, anterior,
			"EXECUTING");
// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
	OperatingSystem_PrintStatus();

}

// Modify hardware registers with appropriate values for the process identified by PID
/**
 * Modificar los registros de hardware con valores apropiados para el proceso identificado por PID
 * Modificado: NO
 */
void OperatingSystem_RestoreContext(int PID) {

// New values for the CPU registers are obtained from the PCB
	Processor_CopyInSystemStack(MAINMEMORYSIZE - 1,
			processTable[PID].copyOfPCRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE - 2,
			processTable[PID].copyOfPSWRegister);

// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);
}

// Function invoked when the executing process leaves the CPU 
/**
 * Guarda el estado del proceso en ejecucion y lo pasa a estado Ready
 * Modificado: V1.12
 */
void OperatingSystem_PreemptRunningProcess() {
// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
// Change the process' state
	OperatingSystem_MoveToTheREADYState(executingProcessID);
// The processor is not assigned until the OS selects another process
	executingProcessID = NOPROCESS;
}

// Save in the process' PCB essential values stored in hardware registers and the system stack
/**
 * Guarda el estado del proceso PID
 * Modificado: V1.13
 */
void OperatingSystem_SaveContext(int PID) {

// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister = Processor_CopyFromSystemStack(
	MAINMEMORYSIZE - 1);

// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister = Processor_CopyFromSystemStack(
	MAINMEMORYSIZE - 2);
}

// Exception management routine
/**
 * Manejador de excepciones
 * Modificado: V2.7
 */
void OperatingSystem_HandleException(int excepcion) { //char const *tipo) {

// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	OperatingSystem_ShowTime(SYSPROC);
//ComputerSystem_DebugMessage(23, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
//Ejercicio 4.2
	char const *string;
	switch (excepcion) {
	case DIVISIONBYZERO:
		string = "division by zero";
		break;
	case INVALIDPROCESSORMODE:
		string = "invalid processor mode";
		break;
	case INVALIDADDRESS:
		string = "invalid address";
		break;
	case INVALIDINSTRUCTION:
		string = "invalid instruction";
		break;
	}

	ComputerSystem_DebugMessage(140, INTERRUPT, executingProcessID,
			programList[processTable[executingProcessID].programListIndex]->executableName,
			string);
//[24] Process [1 - NombreProcesoPid1] has caused an exception (invalid address) and is being terminated

	OperatingSystem_TerminateProcess();
	OperatingSystem_PrintStatus(); //Ejercicio V2.7
}

// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess() {

	OperatingSystem_ReleaseMainMemory();

	int selectedProcess;
	int anterior = processTable[executingProcessID].state;
	processTable[executingProcessID].state = EXIT;
	OperatingSystem_Print_Cambio_Estado(executingProcessID, anterior, "EXIT");

	if (programList[processTable[executingProcessID].programListIndex]->type
			== USERPROGRAM)
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;

	if (numberOfNotTerminatedUserProcesses <= 0) {
		// Simulation must finish 
		OperatingSystem_ReadyToShutdown();
	}
// Select the next process to execute (sipID if no more user processes)
	selectedProcess = OperatingSystem_ShortTermScheduler();
// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);
}

// System call management routine
/**
 * Maneja las llamadas al sistema
 * Modificado: v2.5
 */
void OperatingSystem_HandleSystemCall() {

	int systemCallID;

// Register A contains the identifier of the issued system call
	systemCallID = Processor_GetRegisterA();

	switch (systemCallID) {
	case SYSCALL_PRINTEXECPID:
		// Show message: "Process [executingProcessID] has the processor assigned\n"
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(24, SYSPROC, executingProcessID,
				programList[processTable[executingProcessID].programListIndex]->executableName);
		break;

	case SYSCALL_END:
		// Show message: "Process [executingProcessID] has requested to terminate\n"
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(25, SYSPROC, executingProcessID,
				programList[processTable[executingProcessID].programListIndex]->executableName);
		OperatingSystem_TerminateProcess();
		OperatingSystem_PrintStatus(); //Ejercicio V2.7
		break;

	case SYSCALL_YIELD:
		OperatingSystem_TransferWithEcualPriority();
		break;

	case SYSCALL_SLEEP:
		//Ejercicio 5
		OperatingSystem_Dormir_Proceso_Actual();
		break;
	case SYSCALL_IO:
		OperatingSystem_EntradaSalida();

	}
}

//	Implement interrupt logic calling appropriate interrupt handle
/**
 * Llama al manejador de interrupciones correspondiente
 * Modificado: 4.6
 */
void OperatingSystem_InterruptLogic(int entryPoint) {
	switch (entryPoint) {
	case SYSCALL_BIT: // SYSCALL_BIT=2
		OperatingSystem_HandleSystemCall();
		break;
	case EXCEPTION_BIT: // EXCEPTION_BIT=6
		OperatingSystem_HandleException(INVALIDINSTRUCTION);
		break;
	case CLOCKINT_BIT: // CLOCKINT_BIT=9
		OperatingSystem_HandleClockInterrupt();
		break;
	case IOEND_BIT: //IOEND_BIT=8
		OperatingSystem_HandleIOEndInterrupt(); //TODO V5.1
		//OperatingSystem_DeviceControlerEndIOOperation(); //TODO V5.4.b ya se llama en el procedimiento de antes
		break;
	default:
		OperatingSystem_InvalidSystemCall(entryPoint);
		break;
	}
}

void Test(char const *cadena, int numero) {
//#define ANSI_COLOR_BLUE "\x1b[34m"
//#define ANSI_COLOR_RESET "\x1b[0m"
	char chr;
	printf("\x1b[34m");
	printf("\t%s %d >>", cadena, numero);
	printf("\x1b[0m");
	scanf("%c", &chr);
}

/**
 * Muestra un error cuando una llamada al sistema realizada por un proceso de usuario no sea reconocida como llamada al sistema válida
 * Creado: V4.4
 */
void OperatingSystem_InvalidSystemCall(int entryPoint){
	OperatingSystem_ShowTime(INTERRUPT);
	ComputerSystem_DebugMessage(141, INTERRUPT, executingProcessID,
			programList[processTable[executingProcessID].programListIndex]->executableName,
			entryPoint);
	OperatingSystem_TerminateProcess(); //TODO Ejercicio 4.6
	OperatingSystem_PrintStatus();
}

/**
 * Imprime la lista de procesos listos
 * Modificado: V1.11
 */
void OperatingSystem_PrintReadyToRunQueue() {

	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE);

	int i = 0;
	ComputerSystem_DebugMessage(112, SHORTTERMSCHEDULE, queueNames[0]);
	for (i = 0; i < numberOfReadyToRunProcesses[0]; i++) {
		int identificador = readyToRunQueue[0][i];
		int prioridad = processTable[identificador].priority;
		ComputerSystem_DebugMessage(113, SHORTTERMSCHEDULE, identificador,
				prioridad);
	}
	ComputerSystem_DebugMessage(109, SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(112, SHORTTERMSCHEDULE, queueNames[1]);
	for (i = 0; i < numberOfReadyToRunProcesses[1]; i++) {
		int identificador = readyToRunQueue[1][i];
		int prioridad = processTable[identificador].priority;
		ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, identificador,
				prioridad);
	}
	ComputerSystem_DebugMessage(114, SHORTTERMSCHEDULE);
}

/**
 * Imprime el cambio de estado de un proceso
 */
void OperatingSystem_Print_Cambio_Estado(int ID, int anterior,
		char const *posterior) {
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(110, SYSPROC, ID,
			programList[processTable[ID].programListIndex]->executableName,
			statesNames[anterior], posterior);
}

// Process [1 – progName1] will transfer the control of the processor to process [3 – progName2]
/**
 * Cambia el proceso en ejecucion a uno con la misma prioridad
 * Modificado: V2.7
 */
void OperatingSystem_TransferWithEcualPriority() {
	int IDActual = executingProcessID;
	int priority = processTable[executingProcessID].priority;
//Localizar proceso para cambiar:
	int indexMasAlta = -1;
	int j = 0;
	int i = 0;
	for (j = 0; j < 2; j++) {
		for (i = 0; i < numberOfReadyToRunProcesses[j]; i++) {
			int identificador = readyToRunQueue[j][i];
			int prioridadAux = processTable[identificador].priority;
			if (prioridadAux == priority) {
				indexMasAlta = identificador;
			}
		}
	}
	if (indexMasAlta == -1) {
		return;
	}
//Imprimir el cambio:
	char *nameMasAlta =
			programList[processTable[indexMasAlta].programListIndex]->executableName;
	char *name =
			programList[processTable[executingProcessID].programListIndex]->executableName;
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(115, SHORTTERMSCHEDULE, executingProcessID,
			name, indexMasAlta, nameMasAlta);
//Quitar el procesador al proceso actual:
	OperatingSystem_PreemptRunningProcess();
//Dar el procesador al siguiente proceso:
	OperatingSystem_Dispatch(indexMasAlta);

//Sacar de la readyToRun al proceso actual
	OperatingSystem_ExtractFromReadyToRun(IDActual);

//Ejercicio V2.7
	OperatingSystem_PrintStatus();

}

/**
 * Imprime y aumenta el numero de interrupciones
 * Modificado: V2.6
 */
void OperatingSystem_HandleClockInterrupt() {
	OperatingSystem_ShowTime(INTERRUPT);
	ComputerSystem_DebugMessage(120, INTERRUPT, numberOfClockInterrupts);
	numberOfClockInterrupts = numberOfClockInterrupts + 1;

//Ejercicio V3.3
	int Ready_Antes = numberOfReadyToRunProcesses[0];
	OperatingSystem_WakeUpProcesses();
	OperatingSystem_LongTermScheduler();
	int Ready_Despues = numberOfReadyToRunProcesses[0];
	if (Ready_Antes != Ready_Despues) {
		OperatingSystem_PrintStatus();
		OperatingSystem_CambiarProcesoAlMasPrioritario();
	}
	return;
}

/**
 * Mueve el proceso actual a la cola de dormidos
 * Modificado: V2.5
 */
void OperatingSystem_Dormir_Proceso_Actual() {
//Calcular el whenToWakeUp
	processTable[executingProcessID].whenToWakeUp =
			OperatingSystem_GetWhenToWakeUp();
// Guardar el estado del proceso
	OperatingSystem_SaveContext(executingProcessID);
// Añadir el proceso a la sleepingProcessesQueue
	OperatingSystem_MoveToTheSleepingProcessesQueue(executingProcessID);
// Select the next process to execute (sipID if no more user processes)
	int selectedProcess = OperatingSystem_ShortTermScheduler();
// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);
}



int OperatingSystem_GetWhenToWakeUp() {
	int acumulador = Processor_GetAccumulator(); // valor actual del registro acumulador
	if (acumulador < 0)
		acumulador = 0 - acumulador;
	int whenToWakeUp = acumulador + numberOfClockInterrupts + 1;
	return whenToWakeUp;
}

/**
 * Añade el proceso PID a la cola de dormidos
 * Cambia el estado del proceso PID a dormido
 * Modificado: V2.5
 */
void OperatingSystem_MoveToTheSleepingProcessesQueue(int PID) {
	if (Heap_add(PID, sleepingProcessesQueue, processTable[PID].whenToWakeUp,
			&numberOfSleepingProcesses, PROCESSTABLEMAXSIZE) >= 0) {
		int anterior = processTable[PID].state;
		processTable[PID].state = BLOCKED;
		OperatingSystem_Print_Cambio_Estado(PID, anterior, "BLOCKED");
	}
}

/**
 * Comprueba que haya un proceso a despertar y lo despierta
 * Modificado: V2.6
 */
void OperatingSystem_WakeUpProcesses() {
	int i = 0;
	int procesosDespertados = 0;
	for (i = 0; i < numberOfSleepingProcesses; i++) {
		int identificador = sleepingProcessesQueue[i];
		if (processTable[identificador].whenToWakeUp
				<= numberOfClockInterrupts) {
			/* Si el campo whenToWakeUp de un proceso (o más de uno) de la cola
			 * sleepingProcessesQueue coincide con el número de interrupciones de
			 * reloj ocurridas hasta el momento:
			 */
			//Despertar proceso:
			OperatingSystem_ExtractFromSleepingQueue(identificador);
			//Pasar a estado ready
			OperatingSystem_MoveToTheREADYState(identificador);
			procesosDespertados++;
		}
	}
	if (procesosDespertados > 0) {
		OperatingSystem_PrintStatus();
		OperatingSystem_CambiarProcesoAlMasPrioritario();
	}
}

/**
 * Saca el elemento queueID de la cola de dormidos
 * Modificado: V2.6
 */
int OperatingSystem_ExtractFromSleepingQueue(int queueID) {
	int selectedProcess = NOPROCESS;
	selectedProcess = Heap_poll(sleepingProcessesQueue, QUEUE_WAKEUP,
			&numberOfSleepingProcesses);
	return selectedProcess;
}

/**
 * Cambia el proceso en ejecucion por el que tenga la prioridad mas alta
 * Modificado: V2.6
 */
int OperatingSystem_CambiarProcesoAlMasPrioritario() {
	int IDActual = executingProcessID;
	int prioridadActual = processTable[executingProcessID].priority;
//Localizar proceso para cambiar:
	int indexMasAlta = -1;
	int prioridadMasAlta = -1;
	int i = 0;
	for (i = 0; i < numberOfReadyToRunProcesses[0]; i++) {
		int identificador = readyToRunQueue[0][i];
		int prioridadAux = processTable[identificador].priority;
		if (prioridadAux > indexMasAlta) {
			indexMasAlta = identificador;
			prioridadMasAlta = prioridadAux;
		}
	}
	if (prioridadMasAlta <= prioridadActual) {
		return 0;
	}
//Imprimir el cambio:
	char *nameMasAlta =
			programList[processTable[indexMasAlta].programListIndex]->executableName;
	char *name =
			programList[processTable[executingProcessID].programListIndex]->executableName;
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(121, SHORTTERMSCHEDULE, executingProcessID,
			name, indexMasAlta, nameMasAlta);
//Quitar el procesador al proceso actual:
	OperatingSystem_PreemptRunningProcess();
//Dar el procesador al siguiente proceso:
	OperatingSystem_Dispatch(indexMasAlta);

//Sacar de la readyToRun al proceso actual
	OperatingSystem_ExtractFromReadyToRun(IDActual);

	return 1;
}

/**
 * Devuelve el PID del proceso actual en ejecucion
 * Modificado: V3.1
 */
int OperatingSystem_GetExecutingProcessID() {
	return executingProcessID;
}
/**
 * Libera la memoria del proceso executingProcessID
 * Creado: v4.7
 */
void OperatingSystem_ReleaseMainMemory() {
	int currentPartition = 0;
	//TODO: Ejercicio 4.7
	for (currentPartition = 0; currentPartition < PARTITIONTABLEMAXSIZE;
			currentPartition++) {
		if (partitionsTable[currentPartition].PID == executingProcessID) {
			partitionsTable[currentPartition].occupied = 0;
		}

	}
}

/**
 * Rutina del SO de tratamiento de interrupciones de fin de entrada/salida
 * Modificado V5.5
 */
void OperatingSystem_HandleIOEndInterrupt() {
	//TODO V5.5

	//a. Pedir al manejador dependiente del dispositivo que se disponga a
	//gestionar la siguiente petición pendiente.
	int PID = OperatingSystem_DeviceControlerEndIOOperation();


	//b. Desbloquear al proceso cuya entrada/salida ha finalizado. Como
	//hay cambio de estado de procesos, hay que sacar los mensajes de cambio de
	//estado pertinentes y como implica cambios en las colas, se debe hacer una
	//llamada a OperatingSystem_PrintStatus()

	//int IOWaitingProcessesQueue[PROCESSTABLEMAXSIZE];
	//int numberOfIOWaitingProcesses = 0;
	int anterior = processTable[PID].state;
	processTable[PID].state = READY;
	OperatingSystem_Print_Cambio_Estado(executingProcessID, anterior, "READY");
	OperatingSystem_PrintStatus();

	//Heap_poll(IOWaitingProcessesQueue, QUEUE_WAKEUP,&numberOfIOWaitingProcesses); //TODO no se si es QUEUE_WAKEUP




	//c. Requisar el procesador al proceso en ejecución si fuese necesario,
	//en cuyo caso se llamaría a OperatingSystem_PrintStatus()

	if(OperatingSystem_CambiarProcesoAlMasPrioritario() == 1){
		OperatingSystem_PrintStatus();
	}
}

/**
 * Manejador independiente del dispositivo
 * Creado V5.2
 */
void OperatingSystem_IOScheduler() {
	/**
	 * 1 Al recibir una peticion de E/S desde la rutina de tratamiento de llamadas al sistema (TRAP 1),
	 * 2 Añade la peticion en la cola de peticiones asociadas al dispositivo.
	 * 3 Avisa al manejador dependiente del dispositivo de que hay una nueva peticion.
	 */

	QueueFIFO_add(executingProcessID, IOWaitingProcessesQueue,
			numberOfIOWaitingProcesses, PROCESSTABLEMAXSIZE); //Para añadir al final de la cola.
	//TODO V5.4.a
	OperatingSystem_DeviceControlerStartIOOperation();
}

/**
 * Manejador Demendiente del dispositivo Parte 1
 * Modificado V5.4.a
 */
void OperatingSystem_DeviceControlerStartIOOperation() {//int valueToPrint) {
	//Si el dispositivo está libre, la información que debe enviar al dispositivo para
	//que la muestre (Device_StartIO(value)) será el PID del proceso que solicita la operación.
	if (Device_GetStatus == FREE) { //TODO V5.4.a
		Device_StartIO(
				QueueFIFO_getFirst(IOWaitingProcessesQueue,
						numberOfIOWaitingProcesses));
	}

}

/**
 * Manejador Demendiente del dispositivo Parte 2
 * Modificado V5.4.b
 */
int OperatingSystem_DeviceControlerEndIOOperation() {
	//Se encargará de gestionar la siguiente petición de la cola, en el caso de que exista.
	//Devuelve el PID del proceso que ha terminado su operación de E/S
	//TODO V5.4.b


	//int IOWaitingProcessesQueue[PROCESSTABLEMAXSIZE];
	//int numberOfIOWaitingProcesses = 0;

	int PID = 0; //TODO <<<<<<<<< no se que poner aqui

	if(numberOfIOWaitingProcesses != 0){
		Device_StartIO(QueueFIFO_getFirst(IOWaitingProcessesQueue,numberOfIOWaitingProcesses));
	}
	return PID;
}


/**
 * Es utilizada por los procesos de usuario para realizar entrada/salida sobre el dispositivo
 * Modificado V5.6
 */
void OperatingSystem_EntradaSalida(){
	//a. Bloquear al proceso que realiza la llamada. Como hay cambio de estado
	//se debe mostrar el mensaje de cambio de estado pertinente.
	int PID = executingProcessID;
	OperatingSystem_SaveContext(PID);
	if (Heap_add(PID, IOWaitingProcessesQueue, processTable[PID].queueID,
			&numberOfIOWaitingProcesses, PROCESSTABLEMAXSIZE) >= 0) {
		int anterior = processTable[PID].state;
		processTable[PID].state = BLOCKED;
		OperatingSystem_Print_Cambio_Estado(PID, anterior, "BLOCKED");
	}

	//b. Invocar al manejador independiente del dispositivo para que se ponga en marcha la operación.
	OperatingSystem_DeviceControlerStartIOOperation();


	//c. Hacer una llamada a OperatingSystem_PrintStatus()
	OperatingSystem_PrintStatus();

}
