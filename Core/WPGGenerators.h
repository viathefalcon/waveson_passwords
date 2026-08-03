// WPGGenerators.h: declares the interface for the password-generating functions, classes.
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
#include "exports.h"
#include "BitOps.h"

// Types
//

typedef enum _WPGCap {

	WPGCapNONE = 0,
	WPGCapRDRAND = 1,
	WPGCapTPM12 = 2,
	WPGCapTPM20 = 4,
	WPGCapTest = 0xFF

} WPGCap;

typedef DWORD WPGCaps;

// Class(es)
//

class WPG_CORE_API rng_t {
public:
	typedef unsigned char size_type;

	rng_t(void) = default;
	virtual ~rng_t(void) = default;

	virtual operator WPGCap(void) const = 0;
	virtual operator bool(void) const = 0;

	virtual size_type fill(void*, size_type) = 0;
};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4251) // STL members in exported class are intentional
#endif

class WPG_CORE_API wpg_t {
public:
	virtual ~wpg_t(void) = default;

	// Generates a password in the given output buffer; returns an enumeration of the generators which failed
	WPGCaps Generate(
		__out_ecount(cchBuffer) LPTSTR pszBuffer,
		__in BYTE cchBuffer,
		__in WPGCaps,
		__inout PBYTE,
		__in_z LPCTSTR,
		__in BOOL);

	// Return a token indicating the vector extensions being used by the generator
	XORVex Vex(void) const {
		return m_xor->vex( );
	}

	// Returns a token indicating the generator's capabilities
	WPGCaps Caps(void) const;

	// Instantiates a new generator
	static std::shared_ptr<wpg_t> New(void);

protected:
	wpg_t(::std::vector<::std::unique_ptr<rng_t>>&& p_rngs, ::std::unique_ptr<xor_t> p_xor):
		m_rngs( ::std::move( p_rngs ) ),
		m_xor( ::std::move( p_xor ) ) { }

	::std::vector<::std::unique_ptr<rng_t>> m_rngs;
	::std::unique_ptr<xor_t> m_xor;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// Prototypes
//

// Returns the first set capability of the given collection of capabilities
WPG_CORE_EXTERN_C WPG_CORE_API WPGCap WPGCapsFirst(WPGCaps);

#endif // !defined(__WPG_GENERATORS_H__)
