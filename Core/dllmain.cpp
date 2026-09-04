// dllmain.cpp : Defines the entry point for the DLL.
//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

// Includes
//

// Precompiled Headers
#include "pch.h"

// Declarations
#include "dllmain.h"

// Globals
//

static HINSTANCE g_hInstance = NULL;

// Functions
//

// Gives the entry-point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD  dwReasonForCall, LPVOID lpReserved) {

	switch (dwReasonForCall){
		case DLL_PROCESS_ATTACH:
			g_hInstance = static_cast<HINSTANCE>(hModule);
			break;

		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			// Do nothing
			break;
	}
	return TRUE;
}

HINSTANCE GetCoreInstance(void) {
	return g_hInstance;
}
