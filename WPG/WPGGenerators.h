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

// Class(es)
//

class wpg_t {
public:
	// Generates a password in the given output buffer; returns an enumeration of the generators which failed
	virtual WPGCaps generate(__out_ecount(cchBuffer) LPTSTR pszBuffer,
							 __in BYTE cchBuffer,
							 __in WPGCaps,
							 __inout PBYTE,
							 __in_z LPCTSTR,
							 __in BOOL) = 0;
};

typedef wpg_t* wpg_ptr;

// Prototypes
//

// Returns password-generating capabilities of the current host
WPGCaps WPGGetCaps(VOID);

// Returns the first set capability of the given collection of capabilities
WPGCap WPGCapsFirst(WPGCaps);

// Instantiates a new password generator and returns a pointer to it
wpg_ptr make_new_wpg(void);

// Releases a previously-instantiated password generator
void release_wpg(wpg_ptr);

#endif // !defined(__WPG_GENERATORS_H__)
