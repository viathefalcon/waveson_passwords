// Generators.h: declares the interface for the password-generating classes.
//
// Author: Stephen Higgins
// Blog: http://blog.viathefalcon.net/
// Twitter: @viathefalcon
//

// Includes
//

// Precompiled Headers
#include "Stdafx.h"

// C Standard Library Headers
#include <stdlib.h>

// C++ Standard Library Headers
#include <string>
#include <memory>
#include <vector>
#include <algorithm>

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

// Classes
//

class pwd_generator_t {
public:
	typedef unsigned char size_type;

	pwd_generator_t(void) = default;
	virtual ~pwd_generator_t(void) = default;

	virtual operator WPGCap(void) const = 0;
	virtual operator bool(void) const = 0;

	virtual size_type fill(void*, size_type) = 0;
};

template <WPGCap _cap>
class cap_generator_t: public pwd_generator_t {
public:
	const WPGCap cap = _cap;

	cap_generator_t(void) = default;
	cap_generator_t(const cap_generator_t&) = delete;
	virtual ~cap_generator_t(void) = default;

	cap_generator_t& operator=(const cap_generator_t&) = delete;

	operator WPGCap(void) const {
		return cap;
	}

	virtual size_type fill(void*, size_type) {
		return 0;
	}
};

class rdrand_generator_t: public cap_generator_t<WPGCapRDRAND> {
public:
	operator bool(void) const;
	size_type fill(void*, size_type);
};

rdrand_generator_t::operator bool(void) const {
	return ::rdrand_supported( );
}

rdrand_generator_t::size_type rdrand_generator_t::fill(void* buffer, rdrand_generator_t::size_type size) {

	decltype(size) filled = 0;
	const auto delta = reinterpret_cast<size_t>( buffer ) % sizeof( uint32_t );
	if (delta){
		uint32_t rdrand = 0;
		if (::rdrand_next( &rdrand )){
			std::memcpy( buffer, &rdrand, delta );
			filled += static_cast<decltype(filled)>( delta );
		}else{
			return 0;
		}
	}

	const auto count = size / sizeof( uint32_t );
	auto next = reinterpret_cast<uint32_ptr>( reinterpret_cast<unsigned char*>( buffer ) + delta );
	for (size_type s = 0; s < count; ++s){
		if (::rdrand_next( next )){
			++next;
			filled += sizeof( uint32_t );
		}else{
			return 0;
		}
	}

	const auto rem = size - filled;
	if (rem){
		uint32_t rdrand = 0;
		if (::rdrand_next( &rdrand )){
			std::memcpy( next, &rdrand, rem );
			filled += rem;
		}else{
			return 0;
		}
	}
	return filled;
}

class tpm12_generator_t: public cap_generator_t<WPGCapTPM12> {
public:
	tpm12_generator_t(void);
	virtual ~tpm12_generator_t(void);

	operator bool(void) const {
		return (m_hContext != NULL);
	}

	size_type fill(void*, size_type);

private:
	TBS_HCONTEXT m_hContext;
};

tpm12_generator_t::tpm12_generator_t(void): m_hContext( NULL ) {

	TBS_CONTEXT_PARAMS contextParams = { 0 };
	contextParams.version = TBS_CONTEXT_VERSION_ONE;
	HRESULT hResult = ::Tbsi_Context_Create( &contextParams, &m_hContext );
	if (FAILED(hResult)){
		m_hContext = NULL;
	}
}

tpm12_generator_t::~tpm12_generator_t(void) {

	if (m_hContext){
		::Tbsip_Context_Close( m_hContext );
	}
}

tpm12_generator_t::size_type tpm12_generator_t::fill(void* buffer, tpm12_generator_t::size_type size) {

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

	size_type result = 0;
	while (size > result){
		const auto requested = size - result;
		UINT32 uBe32 = host_to_be32( requested );
		CopyMemory( bCmd + (cbCmd - sizeof( uBe32 )), &uBe32, sizeof( uBe32 ) );

		// Submit the command
		auto cbResult = cbBuffer;
		HRESULT hResult = ::Tbsip_Submit_Command( m_hContext, TBS_COMMAND_LOCALITY_ZERO, TBS_COMMAND_PRIORITY_NORMAL, bCmd, cbCmd, pBuffer, &cbResult );
		if (SUCCEEDED( hResult )){
			// Get out the number of random bytes returned from the TPM
			const SIZE_T cbSize = sizeof( uBe32 );
			CopyMemory( &uBe32, pBuffer + (cbCmd - cbSize), cbSize );
			const auto generated = be32_to_host( uBe32 );
			if (generated){
				// Copy the generated random data to the output buffer
				CopyMemory( static_cast<unsigned char*>( buffer ) + result, pBuffer + cbCmd, generated );
				result += static_cast<decltype(result)>( generated );
				continue;
			}
		}
		result = 0;
		break;
	}
	PH_FREE( pBuffer );
	return result;
}

class tpm20_generator_t: public cap_generator_t<WPGCapTPM20> {
public:
	tpm20_generator_t(void);
	~tpm20_generator_t(void);

	operator bool(void) const {
		return (m_hContext != NULL);
	}

	size_type fill(void*, size_type);

private:
	TBS_HCONTEXT m_hContext;
};

tpm20_generator_t::tpm20_generator_t(void): m_hContext( NULL ) {

	TBS_CONTEXT_PARAMS2 contextParams = { 0 };
	contextParams.version = TBS_CONTEXT_VERSION_TWO;
	contextParams.includeTpm12 = 0;
	contextParams.includeTpm20 = 1;
	HRESULT hResult = ::Tbsi_Context_Create( reinterpret_cast<PCTBS_CONTEXT_PARAMS>( &contextParams ), &m_hContext );
	if (FAILED( hResult )){
		m_hContext = NULL;
	}
}

tpm20_generator_t::~tpm20_generator_t(void) {

	if (m_hContext){
		::Tbsip_Context_Close( m_hContext );
	}
}

tpm20_generator_t::size_type tpm20_generator_t::fill(void* buffer, tpm20_generator_t::size_type size) {

#pragma pack(push,1)
	typedef struct _tpm20_get_random_t {
		unsigned short tag;
		unsigned long size;
		unsigned long code;
		unsigned short param;
	} tpm20_get_random_t;
#pragma pack(pop,1)

	// Allocate a buffer big enough for the command and the output
	const UINT32 cbCmd = sizeof( tpm20_get_random_t );
	UINT32 cbBuffer = cbCmd + static_cast<UINT32>( size );
	auto pBuffer = static_cast<PBYTE>( PH_ALLOC( cbBuffer ) );

	size_type result = 0;
	for (unsigned long rc = 0; (rc == 0) && (size > result); ){
		const size_type requested = size - result;

		tpm20_get_random_t cmd = { 0 };
		cmd.tag = host_to_be16( TPM2_ST_NO_SESSIONS );
		cmd.size = host_to_be32( sizeof( cmd ) );
		cmd.code = host_to_be32( TPM2_CC_GET_RANDOM );
		cmd.param = host_to_be16( static_cast<unsigned short>( requested ) );
		ZeroMemory( pBuffer, cbBuffer );
		CopyMemory( pBuffer, &cmd, cbCmd );

		// Submit the command
		auto cbResult = cbBuffer;
		HRESULT hResult = ::Tbsip_Submit_Command( m_hContext, TBS_COMMAND_LOCALITY_ZERO, TBS_COMMAND_PRIORITY_NORMAL, pBuffer, cbCmd, pBuffer, &cbResult );
		if (SUCCEEDED( hResult )){
			// Check the response code
			UINT32 uBe32 = 0;
			CopyMemory( &uBe32, pBuffer + offsetof( tpm20_get_random_t, code ), sizeof( uBe32 ) );
			rc = be32_to_host( uBe32 ); // Yeah, not necessary..
			if (rc == 0){
				CopyMemory( &uBe32, pBuffer + offsetof( tpm20_get_random_t, size ), sizeof( uBe32 ) );
				const auto generated = min( be32_to_host( uBe32 ) - cbCmd, requested );
				/*
#if defined (_DEBUG)
				TCHAR szDebug[MAX_PATH] = { 0 };
				for (decltype(result) i = 0; i < result; i++){
					StringCchPrintf( szDebug, MAX_PATH, TEXT( "0x%02x " ), *(pBuffer + cbCmd + i) );
					OutputDebugString( szDebug );
				}
				OutputDebugString( TEXT( "\x0a" ) );
#endif
				*/
				CopyMemory( static_cast<unsigned char*>( buffer ) + result, pBuffer + cbCmd, generated );
				result += static_cast<decltype(result)>( generated );
				continue;
			}
		}

		// Fail if we get here
		result = 0;
		break;
	}
	PH_FREE( pBuffer );
	return result;
}

// Functions
//

typedef std::vector<std::unique_ptr<pwd_generator_t>> pwd_generator_vector_t;
static pwd_generator_vector_t get_pwd_generators(WPGCaps caps) {

	pwd_generator_vector_t generators;
	if (caps & WPGCapRDRAND){
		auto rdrand = std::make_unique<rdrand_generator_t>( );
		if (rdrand && *rdrand){
			generators.push_back( std::move( rdrand ) );
		}
	}
	if (caps & WPGCapTPM12){
		auto tpm12 = std::make_unique<tpm12_generator_t>( );
		if (tpm12 && *tpm12){
			generators.push_back( std::move( tpm12 ) );
		}
	}
	if (caps & WPGCapTPM20){
		auto tpm20 = std::make_unique<tpm20_generator_t>( );
		if (tpm20 && *tpm20){
			generators.push_back( std::move( tpm20 ) );
		}
	}
	return generators;
}

WPGCaps WPGGetCaps(VOID) {

	const WPGCaps wpgCapsAll = WPGCapRDRAND | WPGCapTPM12 | WPGCapTPM20;
	auto generators = get_pwd_generators( wpgCapsAll );

	WPGCaps caps = WPGCapNONE;
	std::for_each( generators.cbegin( ), generators.cend( ), [&](const decltype(generators)::value_type& generator) {
		caps |= static_cast<WPGCap>( *generator );
	} );

	return caps;
}

WPGCap WPGCapsFirst(WPGCaps caps) {

	DWORD dw = 1;
	while (caps && !(caps & 1)){
		caps >>= 1;
		dw <<= 1;
	}
	return static_cast<WPGCap>(caps ? dw : 0);
}

WPGCaps WPGPwdGen(__out_ecount(cchBuffer) LPTSTR pszBuffer, __in SIZE_T cchBuffer, __in WPGCaps caps, __inout PSIZE_T cchLength, __in_z LPCTSTR pszAlphabet) {

	// Allocate a pair of buffers 
	LPBYTE lpFront = static_cast<LPBYTE>( PH_ALLOC( cchBuffer ) );
	LPBYTE lpBack = static_cast<LPBYTE>( PH_ALLOC( cchBuffer ) );

	WPGCaps result = WPGCapNONE;
	auto generators = get_pwd_generators( caps );
	std::for_each( generators.cbegin( ), generators.cend( ), [&](const decltype(generators)::value_type& generator) {
		using size_type = decltype(generators)::value_type::element_type::size_type;
		auto filled = generator->fill( lpBack, static_cast<size_type>( cchBuffer ) );
		if (filled){
			// Xor with previously-generated values (if any)
			for (decltype(filled) i = 0; i < filled; ++i){
				*(lpFront + i) ^= *(lpBack + i);
			}
		}else{
			result |= static_cast<WPGCap>( *generator );
		}
	} );

	// Use the contents of the front buffer as an index into the alphabet, to generate the password
	size_t cchAlphabet = 0;
	StringCchLength( pszAlphabet, STRSAFE_MAX_CCH, &cchAlphabet );
	for (decltype(cchBuffer) i = 0; i < cchBuffer; ++i){
		const auto rand = static_cast<decltype(cchAlphabet)>( *(lpFront + i) );
		const auto index = rand % cchAlphabet;
		*(pszBuffer + i) = *(pszAlphabet + index);
	}
	if (cchLength){
		*cchLength = cchBuffer;
	}

	// Cleanup
	PH_FREE( lpFront );
	PH_FREE( lpBack );
	return result;
}
