// WPGGenerators.cpp: defines the implementation for the password-generating functions, classes.
//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

// Includes
//

// Precompiled Headers
#include "pch.h"

// C++ Standard Library Headers
#include <string>
#include <memory>
#include <vector>
#include <algorithm>

// TPM Services Headers
#include <tbs.h>

// RDRAND Headers
#include "ia_rdrand.h"

// Local Project Headers
#include "heaps.h"

// Declarations
#include "WPGGenerators.h"

// Macros
//

#if defined (_M_IX86) || defined (_M_X64) || defined (_M_ARM64)
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

// Forward Declarations
//

SIZE_T RdRandFill(PVOID, const SIZE_T);

// Classes
//

template <WPGCap _cap>
class cap_rng_t: public rng_t {
public:
	const WPGCap cap = _cap;

	cap_rng_t(void) = default;
	cap_rng_t(const cap_rng_t&) = delete;
	virtual ~cap_rng_t(void) = default;

	cap_rng_t& operator=(const cap_rng_t&) = delete;

	operator WPGCap(void) const {
		return cap;
	}

	virtual size_type fill(void*, size_type) {
		return 0;
	}
};

class rdrand_rng_t: public cap_rng_t<WPGCapRDRAND> {
public:
	operator bool(void) const {
		return ::rdrand_supported( );
	}

	size_type fill(void* buffer, rdrand_rng_t::size_type size) {
		return static_cast<size_type>( ::RdRandFill( buffer, size ) );
	}
};

class tpm12_rng_t: public cap_rng_t<WPGCapTPM12> {
public:
	tpm12_rng_t(void);
	virtual ~tpm12_rng_t(void);

	operator bool(void) const {
		return (m_hContext != NULL);
	}

	size_type fill(void*, size_type);

private:
	TBS_HCONTEXT m_hContext;
};

tpm12_rng_t::tpm12_rng_t(void): m_hContext( NULL ) {

	TBS_CONTEXT_PARAMS contextParams = { 0 };
	contextParams.version = TBS_CONTEXT_VERSION_ONE;
	HRESULT hResult = ::Tbsi_Context_Create( &contextParams, &m_hContext );
	if (FAILED(hResult)){
		m_hContext = NULL;
	}
}

tpm12_rng_t::~tpm12_rng_t(void) {

	if (m_hContext){
		::Tbsip_Context_Close( m_hContext );
	}
}

tpm12_rng_t::size_type tpm12_rng_t::fill(void* buffer, tpm12_rng_t::size_type size) {

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

class tpm20_rng_t: public cap_rng_t<WPGCapTPM20> {
public:
	tpm20_rng_t(void);
	~tpm20_rng_t(void);

	operator bool(void) const {
		return (m_hContext != NULL);
	}

	size_type fill(void*, size_type);

private:
	TBS_HCONTEXT m_hContext;
};

tpm20_rng_t::tpm20_rng_t(void): m_hContext( NULL ) {

	TBS_CONTEXT_PARAMS2 contextParams = { 0 };
	contextParams.version = TBS_CONTEXT_VERSION_TWO;
	contextParams.includeTpm12 = 0;
	contextParams.includeTpm20 = 1;
	HRESULT hResult = ::Tbsi_Context_Create( reinterpret_cast<PCTBS_CONTEXT_PARAMS>( &contextParams ), &m_hContext );
	if (FAILED( hResult )){
		m_hContext = NULL;
	}
}

tpm20_rng_t::~tpm20_rng_t(void) {

	if (m_hContext){
		::Tbsip_Context_Close( m_hContext );
	}
}

tpm20_rng_t::size_type tpm20_rng_t::fill(void* buffer, tpm20_rng_t::size_type size) {

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

template <typename word_type>
static SIZE_T round_up_byte_count(SIZE_T byte_count)
{
	return ((byte_count + sizeof(word_type) - 1) / sizeof(word_type)) * sizeof(word_type);
}

_Use_decl_annotations_
WPGCaps wpg_t::Generate(LPTSTR pszBuffer,
						BYTE cchBuffer,
						WPGCaps caps,
						PBYTE cchLength,
						LPCTSTR pszAlphabet,
						BOOL fDuplicatesAllowed) {
	// Setup
	size_t cchAlphabet = 0;
	StringCchLength( pszAlphabet, STRSAFE_MAX_CCH, &cchAlphabet );
	std::unique_ptr<empty_bitset_t> bitset = (fDuplicatesAllowed)
		? std::make_unique<empty_bitset_t>( cchAlphabet )
		: std::make_unique<bitset_t>( cchAlphabet );
	selector_t<uintmax_t> selector( 0, cchAlphabet );

	// Allocate a pair of buffers - rounding up to the next nearest number
	// of whole words
	const auto cbBuffer = round_up_byte_count<uintmax_t>( cchBuffer );
#if defined (_DEBUG)
	TCHAR szBuf[128];
	::StringCchPrintf(szBuf, 128, TEXT("Allocating %d bytes for %u characters.\x0A"), cbBuffer, cchBuffer);
	OutputDebugString( szBuf );
#endif
	LPBYTE lpFront = static_cast<LPBYTE>( PH_ALLOC( cbBuffer ) );
	LPBYTE lpBack = static_cast<LPBYTE>( PH_ALLOC( cbBuffer ) );

	// Loop until we've emitted the requested number of characters
	// (or determine we can't)
	decltype(cchBuffer) cchFilled = 0;
	WPGCaps wpgCapsFailed = WPGCapNONE;
	while ((cchFilled < cchBuffer) && (wpgCapsFailed == WPGCapNONE)){
		using rng_type = decltype(m_rngs)::value_type;
		using size_type = rng_type::element_type::size_type;

		// Generate some new random values
		const auto cchUnfilled = static_cast<size_type>(cchBuffer - cchFilled);
		auto generated = round_up_byte_count<uintmax_t>( cchUnfilled );
		::std::for_each( m_rngs.cbegin( ), m_rngs.cend( ), [&](const rng_type& rng) {
			const auto cap = static_cast<WPGCap>( *rng );
			if ((caps & cap) == 0){
				// The caller hasn't asked to use the current RNG, so just skip over it
				return;
			}

			const auto filled = rng->fill( lpBack, static_cast<size_type>( cbBuffer ) );
			if (filled){
				// Xor with previously-generated values (if any)
				generated = min( generated, static_cast<decltype(generated)>( filled ) );
				m_xor->apply( lpFront, lpBack, generated );
			}else{
				wpgCapsFailed |= cap;
			}
		} );
		if (wpgCapsFailed == WPGCapNONE){
#if defined (_DEBUG)
			{
				TCHAR szBuf[128];
				::StringCchPrintf(szBuf, 128, TEXT("Generated %d random bytes for %u remaining characters.\x0A"), generated, cchUnfilled);
				OutputDebugString( szBuf );
			}
#endif

			// Use the contents of the front buffer as an index into the alphabet, to generate the password
			for (size_t remaining = generated, offset = 0; remaining > 0; ){
				// Pack the bytes from the RNG(s) into a primitive on which we can perform bitwise ops
				decltype(selector)::value_type word = 0;
				const auto copyable = min(sizeof( word ), remaining);

				// Put the first next-value into the least-significant byte of word,
				// the next-next-value into the next byte, etc. (host-endian agnostic).
				for (size_t i = 0, j = 0; i < copyable; ++i, j += CHAR_BIT) {
					const auto next = *(lpFront + offset++);
					word |= (static_cast<decltype(word)>(next) << j);
#if defined (_DEBUG)
					OutputDebugStringA("Got: " );
					OutputDebugStringA( bits_to_string( next ).c_str( ) );
					OutputDebugStringA( "; applied: " );
					OutputDebugStringA( bits_to_string( word ).c_str( ) );
					OutputDebugStringA("\x0A" );
#endif
				}

				// Apply
				bool not_finished = true;
#if defined (_DEBUG)
				size_t counter = 0;
#endif
				for (selector.reset( word ); not_finished && selector.has_next(); ){
					const auto index = selector.next();
#if defined (_DEBUG)
					++counter;
#endif
					if (bitset->is_set( index )){
#if defined (_DEBUG)
						TCHAR szBuf[2] = { *(pszAlphabet + index), NULL };
						OutputDebugString( TEXT( "Already have: " ) );
						OutputDebugString( szBuf );
						OutputDebugString( TEXT( ". Skipping..\x0A" ) );
#endif
						continue;
					}

					*(pszBuffer + (cchFilled++)) = *(pszAlphabet + index);
					bitset->set( index );

					// Test whether we need to go again
					not_finished = (cchFilled < cchBuffer);
				}
#if defined (_DEBUG)
				{
					TCHAR szBuf[128];
					::StringCchPrintf(szBuf, 128, TEXT("Found %llu usable indices in %llu random bytes.\x0A"), counter, copyable);
					OutputDebugString( szBuf );
				}
#endif

				// Advance, maybe
				if (not_finished){
					remaining -= min(copyable, remaining);
					continue;
				}

				// We're done
				break;
			}
		}

		SecureZeroMemory( lpFront, cbBuffer );
		SecureZeroMemory( lpBack, cbBuffer );
	}
	if (cchLength){
		*cchLength = cchFilled;
	}

	// Cleanup, return
	PH_FREE( lpFront );
	PH_FREE( lpBack );
	return wpgCapsFailed;
}

WPGCaps wpg_t::Caps(void) const {

	WPGCaps caps = WPGCapNONE;
	::std::for_each( m_rngs.cbegin( ), m_rngs.cend( ), [&](const decltype(m_rngs)::value_type& rng) {
		caps |= static_cast<WPGCap>( *rng );
	} );
	return caps;
}

std::shared_ptr<wpg_t> wpg_t::New(void) {

	decltype(wpg_t::m_rngs) rngs;

	auto rdrand = std::make_unique<rdrand_rng_t>( );
	if (rdrand && *rdrand){
		rngs.push_back( std::move( rdrand ) );
	}
	auto tpm20 = std::make_unique<tpm20_rng_t>( );
	if (tpm20 && *tpm20){
		rngs.push_back( std::move( tpm20 ) );

		// If we have TPM 2.0, then we don't need TPM 1.2 (below)
		;
	}else{
		auto tpm12 = std::make_unique<tpm12_rng_t>( );
		if (tpm12 && *tpm12){
			rngs.push_back( std::move( tpm12 ) );
		}
	}

	return std::shared_ptr<wpg_t>(
		new wpg_t( std::move( rngs ), get_vex_xor( ) )
	);
}

// Functions
//

WPG_CORE_EXTERN_C WPG_CORE_API WPGCap WPGCapsFirst(WPGCaps caps) {

	DWORD dw = 1;
	while (caps && !(caps & 1)){
		caps >>= 1;
		dw <<= 1;
	}
	return static_cast<WPGCap>(caps ? dw : 0);
}

static SIZE_T RdRandFill(PVOID buffer, const SIZE_T size) {

	// Firstly, fill any bytes at the start of the buffer, which
	// are not 4-byte aligned, using an additional copy
	SIZE_T filled = 0;
	const SIZE_T delta = reinterpret_cast<SIZE_T>( buffer ) % alignof( uint32_t );
	if (delta){
		uint32_t rdrand = 0;
		if (rdrand_next( &rdrand )){
			CopyMemory( buffer, &rdrand, delta );
			filled += delta;
		}else{
			return 0;
		}
	}

	// Secondly, fill the 4-byte aligned bytes
	SIZE_T cb = (size - filled);
	const SIZE_T count = cb / sizeof( uint32_t );
	uint32_ptr next = reinterpret_cast<uint32_ptr>( reinterpret_cast<PBYTE>( buffer ) + filled );
	for (SIZE_T s = 0; s < count; ++s){
		if (rdrand_next( next )){
			++next;
			filled += sizeof( uint32_t );
		}else{
			return 0;
		}
	}

	// Finally, fill any remaining bytes
	cb -= filled;
	if (cb){
		uint32_t rdrand = 0;
		if (rdrand_next( &rdrand )){
			SIZE_T bytes = min( cb, sizeof( rdrand ) );
			CopyMemory( reinterpret_cast<PBYTE>( buffer ) + filled, &rdrand, bytes );
			filled += bytes;
		}else{
			return 0;
		}
	}
	return filled;
}
