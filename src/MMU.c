#include "MMU.h"
#include "Buses.h"
#include "Processor.h"
#include <string.h>

// The base register
int registerBase_MMU;

// The limit register
int registerLimit_MMU;

// The MAR register
int registerMAR_MMU;

// Logical address is in registerMAR_MMU. If correct, physical address is produced
// by adding logical address and base register
int MMU_readMemory() {
	if (MMU_VerIntervalo() == MMU_FAIL) { //Ejercicio v4.1
		return MMU_FAIL;
	}

	if (Processor_PSW_BitState(EXECUTION_MODE_BIT)) { // Protected mode
		if (registerMAR_MMU < MAINMEMORYSIZE) {
			// Send to the main memory HW the physical address to write in
			Buses_write_AddressBus_From_To(MMU, MAINMEMORY);
			// Tell the main memory HW to read
			MainMemory_readMemory();
			return MMU_SUCCESS;
		} else {
			return MMU_FAIL;
		}
	} else // Non-Protected mode
	if (registerMAR_MMU < registerLimit_MMU) {
		// Physical address = logical address + base register
		registerMAR_MMU += registerBase_MMU;
		// Send to the main memory HW the physical address to write in
		Buses_write_AddressBus_From_To(MMU, MAINMEMORY);
		// Tell the main memory HW to read
		MainMemory_readMemory();
		return MMU_SUCCESS;
	} else {
		return MMU_FAIL;
	}

}

// Logical address is in registerMAR_MMU. If correct, physical address is produced
// by adding logical address and base register
int MMU_writeMemory() {
	if (MMU_VerIntervalo() == MMU_FAIL) { //Ejercicio v4.1
		return MMU_FAIL;
	}

	if (Processor_PSW_BitState(EXECUTION_MODE_BIT)) // Protected mode
		if (registerMAR_MMU < MAINMEMORYSIZE) {
			// Send to the main memory HW the physical address to write in
			Buses_write_AddressBus_From_To(MMU, MAINMEMORY);
			// Tell the main memory HW to read
			MainMemory_writeMemory();
			return MMU_SUCCESS;
		} else {
			return MMU_FAIL;
		}
	else   // Non-Protected mode
	if (registerMAR_MMU < registerLimit_MMU) {
		// Physical address = logical address + base register
		registerMAR_MMU += registerBase_MMU;
		// Send to the main memory HW the physical address to read from
		Buses_write_AddressBus_From_To(MMU, MAINMEMORY);
		// Tell the main memory HW to write
		MainMemory_writeMemory();
		return MMU_SUCCESS;
	} else {
		return MMU_FAIL;
	}
}

/*
 * Modifica el código de tu MMU para que tenga en cuenta que el rango
 * de direcciones lógicas válidas para el esquema de particiones fijas
 * está comprendido en el intervalo [0, tamañoProceso-1]. Si la
 * dirección lógica está fuera de ese intervalo, la MMU tendrá que
 * elevar una excepción del tipo INVALIDADDRESS.
 */
int MMU_VerIntervalo() {
	// registerLimit_MMU se establece desde OperatingSystem_RestoreContext
	// deberia estar bien pero ya lo hace arriba, asi que no se
	int tamanioProceso = 1;
	if (registerMAR_MMU < 0 || registerMAR_MMU > registerLimit_MMU) {
		Processor_RaiseException(INVALIDADDRESS);
		return MMU_FAIL;
	}
	return MMU_SUCCESS;
}

// Setter for registerMAR_MMU
void MMU_SetMAR(int newMAR) {
	registerMAR_MMU = newMAR;
}

// Getter for registerMAR_MMU
int MMU_GetMAR() {
	return registerMAR_MMU;
}

// Setter for registerBase_MMU
void MMU_SetBase(int newBase) {
	registerBase_MMU = newBase;
}

// Getter for registerBase_MMU
int MMU_GetBase() {
	return registerBase_MMU;
}

// Setter for registerLimit_MMU
void MMU_SetLimit(int newLimit) {
	registerLimit_MMU = newLimit;
}

// Getter for registerLimit_MMU
int MMU_GetLimit() {
	return registerLimit_MMU;
}
