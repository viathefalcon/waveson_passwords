// WPGGenerators.cpp: defines the implementation for the password-generating functions.
//
// Waveson Password Generator
// Web: http://waveson.com/wpg/
// Twitter: @waveson
//
// Author: Stephen Higgins
// Blog: http://blog.viathefalcon.net/
// Twitter: @viathefalcon
//

// Includes
//

// Precompiled Headers
#include "Stdafx.h"

// TPM Services Headers
#include <tbs.h>

// RDRAND Headers
#include "ia_rdrand.h"

// Declarations
#include "WPGGenerators.h"

// Macros
//

#if defined (_M_IX86) || defined (_M_X64)
#define le32_to_host(val) (val)
#define be32_to_host(val) _byteswap_ulong(val)
#define le16_to_host(val) (val)
#define be16_to_host(val) _byteswap_ushort(val)
#define host_to_le32(val) (val)
#define host_to_be32(val) _byteswap_ulong(val)
#define host_to_le16(val) (val)
#define host_to_be16(val) _byteswap_ushort(val)
#else
#error Big/little endianess conversion macros not defined!
#endif

// Constants
//

enum {
	TPM2_ST_NO_SESSIONS	= 0x8001,
};

enum {
	TPM2_CC_GET_RANDOM	= 0x017B,
};

// Types
//

typedef BYTE (*WPGFill)(PVOID, BYTE);

// Functions
//

// Creates and returns a handle to a context for TPM 1.2, or NULL if one could not be created
TBS_HCONTEXT TPMCreateContext12(void) {

	TBS_HCONTEXT hContext = NULL;
	TBS_CONTEXT_PARAMS contextParams = { 0 };
	contextParams.version = TBS_CONTEXT_VERSION_ONE;
	HRESULT hResult = Tbsi_Context_Create( &contextParams, &hContext );
	if (FAILED( hResult )){
		hContext = NULL;
	}
	return hContext;
}

BYTE TPMFill12(PVOID buffer, BYTE size) {

	TBS_HCONTEXT hContext = TPMCreateContext12( );
	if (hContext == NULL){
		return 0;
	}

	BYTE bCmd[] = {
		0x00, 0xc1,					// TPM_TAG_RQU_COMMAND
		0x00, 0x00, 0x00, 0x0e,		// blob length in bytes
		0x00, 0x00, 0x00, 0x46,		// TPM API code (TPM_ORD_GetRandom)
		0x00, 0x00, 0x00, 0x00		// # Bytes (copied in below)
	};
	const UINT32 cbCmd = sizeof( bCmd );

	// Allocate the buffer for the result
	const UINT32 cbBuffer = cbCmd + static_cast<UINT32>( size );
	PBYTE pBuffer = static_cast<PBYTE>( PH_ALLOC( cbBuffer ) );

	BYTE result = 0;
	while (size > result){
		const BYTE requested = size - result;
		UINT32 uBe32 = host_to_be32( requested );
		CopyMemory( bCmd + (cbCmd - sizeof( uBe32 )), &uBe32, sizeof( uBe32 ) );

		// Submit the command
		auto cbResult = cbBuffer;
		HRESULT hResult = Tbsip_Submit_Command( hContext, TBS_COMMAND_LOCALITY_ZERO, TBS_COMMAND_PRIORITY_NORMAL, bCmd, cbCmd, pBuffer, &cbResult );
		if (SUCCEEDED( hResult )){
			// Get out the number of random bytes returned from the TPM
			const SIZE_T cbSize = sizeof( uBe32 );
			CopyMemory( &uBe32, pBuffer + (cbCmd - cbSize), cbSize );
			const auto generated = be32_to_host( uBe32 );
			if (generated){
				// Copy the generated random data to the output buffer
				CopyMemory( static_cast<unsigned char*>( buffer ) + result, pBuffer + cbCmd, generated );
				result += static_cast<BYTE>( generated );
				continue;
			}
		}
		result = 0;
		break;
	}

	// Cleanup, return
	Tbsip_Context_Close( hContext );
	PH_FREE( pBuffer );
	return result;
}

// Creates and returns a handle to a context for TPM 2.0, or NULL if one could not be created
TBS_HCONTEXT TPMCreateContext20(void) {

	TBS_HCONTEXT hContext = NULL;
	TBS_CONTEXT_PARAMS2 contextParams = { 0 };
	contextParams.version = TBS_CONTEXT_VERSION_TWO;
	contextParams.includeTpm12 = 0;
	contextParams.includeTpm20 = 1;
	HRESULT hResult = Tbsi_Context_Create( reinterpret_cast<PCTBS_CONTEXT_PARAMS>( &contextParams ), &hContext );
	if (FAILED( hResult )){
		hContext = NULL;
	}
	return hContext;
}

BYTE TPMFill20(PVOID buffer, BYTE size) {

#pragma pack(push,1)
	typedef struct _tpm20_get_random_t {
		unsigned short tag;
		unsigned long size;
		unsigned long code;
		unsigned short param;
	} tpm20_get_random_t;
#pragma pack(pop,1)

	TBS_HCONTEXT hContext = TPMCreateContext20( );
	if (hContext == NULL){
		return 0;
	}

	// Allocate a buffer big enough for the command and the output
	const UINT32 cbCmd = sizeof( tpm20_get_random_t );
	UINT32 cbBuffer = cbCmd + static_cast<UINT32>( size );
	auto pBuffer = static_cast<PBYTE>( PH_ALLOC( cbBuffer ) );

	BYTE result = 0;
	for (unsigned long rc = 0; (rc == 0) && (size > result); ){
		const BYTE requested = size - result;

		tpm20_get_random_t cmd = { 0 };
		cmd.tag = host_to_be16( TPM2_ST_NO_SESSIONS );
		cmd.size = host_to_be32( sizeof( cmd ) );
		cmd.code = host_to_be32( TPM2_CC_GET_RANDOM );
		cmd.param = host_to_be16( static_cast<unsigned short>( requested ) );
		ZeroMemory( pBuffer, cbBuffer );
		CopyMemory( pBuffer, &cmd, cbCmd );

		// Submit the command
		auto cbResult = cbBuffer;
		HRESULT hResult = Tbsip_Submit_Command( hContext, TBS_COMMAND_LOCALITY_ZERO, TBS_COMMAND_PRIORITY_NORMAL, pBuffer, cbCmd, pBuffer, &cbResult );
		if (SUCCEEDED( hResult )){
			// Check the response code
			UINT32 uBe32 = 0;
			CopyMemory( &uBe32, pBuffer + offsetof( tpm20_get_random_t, code ), sizeof( uBe32 ) );
			rc = be32_to_host( uBe32 ); // Yeah, not necessary..
			if (rc == 0){
				CopyMemory( &uBe32, pBuffer + offsetof( tpm20_get_random_t, size ), sizeof( uBe32 ) );
				const auto generated = min( be32_to_host( uBe32 ) - cbCmd, requested );
				CopyMemory( static_cast<unsigned char*>( buffer ) + result, pBuffer + cbCmd, generated );
				result += static_cast<BYTE>( generated );
				continue;
			}
		}

		// Fail if we get here
		result = 0;
		break;
	}

	// Cleanup, return
	PH_FREE( pBuffer );
	Tbsip_Context_Close( hContext );
	return result;
}

BYTE RdRandFill(PVOID buffer, BYTE size) {

	SIZE_T filled = 0, cb = static_cast<SIZE_T>( size );
	const SIZE_T delta = reinterpret_cast<SIZE_T>( buffer ) % sizeof( uint32_t );
	if (delta){
		uint32_t rdrand = 0;
		if (rdrand_next( &rdrand )){
			CopyMemory( buffer, &rdrand, delta );
			filled += delta;
		}else{
			return 0;
		}
	}

	const SIZE_T count = cb / sizeof( uint32_t );
	uint32_ptr next = reinterpret_cast<uint32_ptr>( reinterpret_cast<PBYTE>( buffer ) + delta );
	for (SIZE_T s = 0; s < count; ++s){
		if (rdrand_next( next )){
			++next;
			filled += sizeof( uint32_t );
		}else{
			return 0;
		}
	}

	cb -= filled;
	if (cb){
		uint32_t rdrand = 0;
		if (rdrand_next( &rdrand )){
			CopyMemory( reinterpret_cast<PBYTE>( buffer ) + filled, &rdrand, cb );
			filled += cb;
		}else{
			return 0;
		}
	}
	return static_cast<BYTE>( filled );
}

// Functions
//

WPGCaps WPGGetCaps(VOID) {

	WPGCaps caps = WPGCapNONE;
	if (rdrand_supported( )){
		caps |= WPGCapRDRAND;
	}
	TBS_HCONTEXT hContext = TPMCreateContext12( );
	if (hContext){
		Tbsip_Context_Close( hContext );
		caps |= WPGCapTPM12;
	}
	hContext = TPMCreateContext20( );
	if (hContext){
		Tbsip_Context_Close( hContext );
		caps |= WPGCapTPM20;
	}
	return caps;
}

WPGCap WPGCapsFirst(WPGCaps caps) {

	DWORD dw = WPGCapRDRAND;
	while (caps && !(caps & 1)){
		caps >>= 1;
		dw <<= 1;
	}
	return static_cast<WPGCap>(caps ? dw : 0);
}

WPGCaps WPGPwdGen(__out_ecount(cchBuffer) LPTSTR pszBuffer, __in BYTE cchBuffer, __in WPGCaps wpgCapsRequested, __inout PBYTE cchLength, __in_z LPCTSTR pszAlphabet) {

	// Get the intersection of sources requested with those available
	const WPGCaps wpgCapsSupported = WPGGetCaps( );
	WPGCaps caps = (wpgCapsRequested & wpgCapsSupported);

	// Allocate a pair of buffers 
	LPBYTE lpFront = static_cast<LPBYTE>( PH_ALLOC( cchBuffer ) );
	LPBYTE lpBack = static_cast<LPBYTE>( PH_ALLOC( cchBuffer ) );

	// Iterate the sources of randomness
	WPGCaps wpgCapsFailed = (wpgCapsRequested & ~wpgCapsSupported);
	for (DWORD dw = WPGCapRDRAND; caps; caps >>= 1, dw <<= 1){
		if (caps & 1){
			WPGFill fill = NULL;
			switch (dw){
				case WPGCapRDRAND:
					fill = RdRandFill;
					break;

				case WPGCapTPM12:
					fill = TPMFill12;
					break;

				case WPGCapTPM20:
					fill = TPMFill20;
					break;
			}

			const BYTE filled = (fill ? fill( lpBack, static_cast<BYTE>( cchBuffer ) ) : 0);
			if (filled){
				// Xor with previously-generated values (if any)
				for (BYTE i = 0; i < filled; ++i){
					*(lpFront + i) ^= *(lpBack + i);
				}
			}else{
				wpgCapsFailed |= dw;
			}
		}
	}

	// Use the contents of the front buffer as an index into the alphabet, to generate the password
	if (wpgCapsFailed != wpgCapsRequested){
		size_t cchAlphabet = 0;
		StringCchLength( pszAlphabet, STRSAFE_MAX_CCH, &cchAlphabet );
		for (BYTE i = 0; i < cchBuffer; ++i){
			const size_t rand = static_cast<size_t>( *(lpFront + i) );
			const size_t index = rand % cchAlphabet;
			*(pszBuffer + i) = *(pszAlphabet + index);
		}
		if (cchLength){
			*cchLength = cchBuffer;
		}
	}

	// Cleanup
	PH_FREE( lpFront );
	PH_FREE( lpBack );
	return wpgCapsFailed;
}
