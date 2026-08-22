// WPGOutputCtl.h: declares the interface to the window class for the output control
//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

#if !defined(__WPG_OUTPUT_CTL_H__)
#define __WPG_OUTPUT_CTL_H__

// Includes
//

// Windows Headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Local Project Headers
#include "exports.h"

// Macros
//

#define WPG_OUTPUT_CLASS TEXT("WPGOutput")

// Functions
//

// Registers the output window class against the given instance; returns zero on failure
WPG_CORE_EXTERN_C WPG_CORE_API ATOM InitWPGOutputControl(__in HINSTANCE);

#endif // __WPG_OUTPUT_CTL_H__
