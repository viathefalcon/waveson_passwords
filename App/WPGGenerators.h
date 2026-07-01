// WPGGenerators.h: declares the interface for the password-generating functions.
//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

#if !defined(__WPG_GENERATORS_H__)
#define __WPG_GENERATORS_H__

// Includes
//

// Windows Headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// C++ Standard Library Headers
#include <memory>

// Local Project Headers
#include "BitOps.h"

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
	virtual WPGCaps Generate(__out_ecount(cchBuffer) LPTSTR pszBuffer,
							 __in BYTE cchBuffer,
							 __in WPGCaps,
							 __inout PBYTE,
							 __in_z LPCTSTR,
							 __in BOOL) = 0;

	// Return a token indicating the vector extensions being used by the generator
	virtual XORVex Vex(void) const {
		return XORVexNONE;
	}

	// Returns a token indicating the generator's capabilities
	virtual WPGCaps Caps(void) const {
		return WPGCapNONE;
	}

	// Instantiates a new generator
	static std::shared_ptr<wpg_t> New(void);
};

typedef wpg_t* wpg_ptr;

// Prototypes
//

// Returns the first set capability of the given collection of capabilities
WPGCap WPGCapsFirst(WPGCaps);

#endif // !defined(__WPG_GENERATORS_H__)
