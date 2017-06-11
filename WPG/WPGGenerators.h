// WPGGenerators.h: declares the interface for the password-generating functions.
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
WPGCaps WPGPwdGen(__out_ecount(cchBuffer) LPTSTR pszBuffer, __in SIZE_T cchBuffer, __in WPGCaps, __inout PSIZE_T, __in_z LPCTSTR);

#endif // !defined(__WPG_GENERATORS_H__)