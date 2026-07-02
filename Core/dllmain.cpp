// dllmain.cpp : Defines the entry point for the DLL.
//

// Includes
//

// Precompiled Headers
#include "pch.h"

// Functions
//

// Gives the entry-point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  dwReasonForCall, LPVOID lpReserved) {

	switch (dwReasonForCall){
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
