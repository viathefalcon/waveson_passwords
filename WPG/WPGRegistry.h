// WPGRegistry.h: declares the interface for functions for working with the windows registry.
//
// Author: Stephen Higgins
// Blog: http://blog.viathefalcon.net/
// Twitter: @viathefalcon
//

#if !defined(__WPG_REGISTRY_H__)
#define __WPG_REGISTRY_H__

// Includes
//

// Windows Headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Macros
//

#define WPGRegCloseKey(key) RegCloseKey( (key) )

// Constants
//

typedef enum _WPGRegValueID {
	WPGRegChecks = 1,
	WPGRegAlphabet = 2,
	WPGRegLength = 3
} WPGRegValueID;

// Prototypes
//

// Opens, and returns, the application-specific registry key
HRESULT WPGRegOpenKey(PHKEY);

// Sets the indicated value to the given double-word
HRESULT WPGRegSetDWORD(HKEY, WPGRegValueID, DWORD);

// Sets the indicated value to the given string
HRESULT WPGRegSetString(HKEY, WPGRegValueID, __in_z LPCTSTR);

// Retrieves the indicated value as a double-word
HRESULT WPGRegGetDWORD(__out LPDWORD, HKEY, WPGRegValueID);

// Retrieves the indicated value as a string (on the process heap)
HRESULT WPGRegGetString(__out LPTSTR*, HKEY, WPGRegValueID);

// Removes the indicated value from the registry
HRESULT WPGRegDelete(HKEY, WPGRegValueID);

#endif // !defined(__WPG_REGISTRY_H__)