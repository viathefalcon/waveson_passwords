// Stdafx.h : include file for standard system include files, or project specific include files that are used frequently, but are changed infrequently
//
// Waveson Password Generator
// Web: http://waveson.com/wpg/
// Twitter: @waveson
//
// Author: Stephen Higgins
// Blog: http://blog.viathefalcon.net/
// Twitter: @viathefalcon
//

#pragma once

// Includes
//

// Local Project Headers
#include "TargetVer.h"
#include "Resource.h"

// Windows Headers
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

// Common Control Headers
#include <commctrl.h>

// COM Headers
#include <objbase.h>
#include <comutil.h>

// Safe String Headers
#include <strsafe.h>

// Macros
//

// Macros to safely release and/or delete interfaces and/or interface pointers.
#ifndef SAFE_DELETE
#define SAFE_DELETE(I) { if(I) { delete[] (I); (I)=NULL; } }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(I) { if(I) { (I)->Release(); (I)=NULL; } }
#endif

// Macros to allocate and release memory from the process heap
#define PH_ALLOC(Bytes) ::HeapAlloc( ::GetProcessHeap( ), HEAP_ZERO_MEMORY, (Bytes) )
#define PH_FREE(Buffer) ::HeapFree( ::GetProcessHeap( ), 0, (Buffer) )
