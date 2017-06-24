// WPGGenerators.h: declares the interface for the password-generating functions.
//
// Waveson Password Generator
// Web: http://waveson.com/wpg/
// Twitter: @waveson
//
// Author: Stephen Higgins
// Blog: http://blog.viathefalcon.net/
// Twitter: @viathefalcon
//

#if !defined(__WPG_GENERATORS_H__)
#define __WPG_GENERATORS_H__

// Includes
//

// Windows Headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Types
//

typedef enum _WPGCap {

	WPGCapNONE = 0,
	WPGCapRDRAND = 1,
	WPGCapTPM12 = 2,
	WPGCapTPM20 = 4,

} WPGCap;

typedef DWORD WPGCaps;

// Prototypes
//

WPGCaps WPGGetCaps(VOID);

WPGCap WPGCapsFirst(WPGCaps);

// Generates a password in the given output buffer; returns an enumeration of the generators which failed
WPGCaps WPGPwdGen(__out_ecount(cchBuffer) LPTSTR pszBuffer, __in BYTE cchBuffer, __in WPGCaps, __inout PBYTE, __in_z LPCTSTR);

#endif // !defined(__WPG_GENERATORS_H__)