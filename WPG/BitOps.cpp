// BitOps.cpp: defines classes, etc., for operating on individual bits
//			   or collections thereof
//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

// Includes
//

// Precompiled Headers
#include "Stdafx.h"

// C++ Standard Library Headers
#include <set>
#include <array>
#include <algorithm>

// Intrinsics Headers
#include <intrin.h>
#if defined(_M_ARM64)
#include <arm_neon.h>
#else
#include <xmmintrin.h>
#endif  //!defined(_M_ARM64)

// Local Project Headers
#include "BitOps.h"

// Classes
//

#if !defined (_WIN64)
class mmx_xor_t : public xor_t {
public:
	size_type apply(operand_type front, operand_type back, size_type cb) const {

		for (decltype(cb) i = 0; i < cb; ){
			const decltype(i) j = min( (cb - i), sizeof( __m64 ) );

			// Get the next chunk
			__m64 mm1 = { 0 }, mm2 = { 0 };
			memcpy( &mm1, (front + i), j );
			memcpy( &mm2, (back + i), j );

			// Apply, and copy out
			__m64 mm3 = _m_pxor( mm1, mm2 );
			memcpy( (front + i), &mm3, j );
			i += j;
		}
		_mm_empty( );
		return cb;
	}

	XORVex vex() const {
		return XORVexMMX;
	}
};
#endif // _WIN64

#if defined(_M_ARM64)
class neon_xor_t : public xor_t {
public:
	size_type apply(operand_type front, operand_type back, size_type cb) const {

		const size_t s = sizeof( uint8x16_t );
		for (decltype(cb) i = 0; i < cb; ) {
			const decltype(cb) j = (cb - i);
			if (j < s){
				// Fallback
				xor_t::apply( front + i, back + i, j );
				i += j;
			}else{
				// Load
				uint8x16_t u = vld1q_u8( front + i );
				uint8x16_t v = vld1q_u8( back + i );

				// Apply and store
				vst1q_u8( front + i, veorq_u8( u, v ) );

				// Advance
				i += s;
			}
		}
		return cb;
	}

	XORVex vex() const {
		return XORVexNEON;
	}
};
#else
class sse_xor_t : public xor_t {
public:
	size_type apply(operand_type front, operand_type back, size_type cb) const {

		const size_t s = sizeof( __m128 );
		for (decltype(cb) i = 0; i < cb; ) {
			const decltype(cb) j = (cb - i);
			if (j < s){
				// Fallback
				xor_t::apply( front + i, back + i, j );
				i += j;
			}else{
				// Load
				auto f = reinterpret_cast<float*>( front + i );
				__m128 u = _mm_loadu_ps( f );
				__m128 v = _mm_loadu_ps( reinterpret_cast<const float*>( back + i ) );

				// Apply and store
				_mm_storeu_ps( f, _mm_xor_ps( u, v ) );

				// Advance
				i += s;
			}
		}
		return cb;
	}

	XORVex vex() const {
		return XORVexSSE;
	}
};

class sse2_xor_t : public xor_t {
public:
	size_type apply(operand_type front, operand_type back, size_type cb) const {

		const size_t s = sizeof( __m128i );
		for (decltype(cb) i = 0; i < cb; ) {
			const decltype(cb) j = (cb - i);
			if (j < s){
				// Fallback
				xor_t::apply( front + i, back + i, j );
				i += j;
			}else{
				// Load
				auto f = reinterpret_cast<__m128i*>( front + i );
				__m128i u = _mm_loadu_si128( f );
				__m128i v = _mm_loadu_si128( reinterpret_cast<__m128i*>( back + i ) );

				// Apply and store
				_mm_storeu_si128( f, _mm_xor_si128( u, v ) );

				// Advance
				i += s;
			}
		}
		return cb;
	}

	XORVex vex() const {
		return XORVexSSE2;
	}
};

class avx_xor_t : public xor_t {
public:
	size_type apply(operand_type front, operand_type back, size_type cb) const {

		const size_t s = sizeof( __m256 );
		for (decltype(cb) i = 0; i < cb; ) {
			const decltype(cb) j = (cb - i);
			if (j < s){
				// Fallback
				xor_t::apply( front + i, back + i, j );
				i += j;
			}else{
				// Load
				auto f = reinterpret_cast<float*>( front + i );
				__m256 u = _mm256_loadu_ps( f );
				__m256 v = _mm256_loadu_ps( reinterpret_cast<float*>( back + i ) );

				// Apply and store
				_mm256_storeu_ps( f, _mm256_xor_ps( u, v ) );

				// Advance
				i += s;
			}
		}
		return cb;
	}

	XORVex vex() const {
		return XORVexAVX;
	}
};
#endif // defined(_M_ARM64)

// Functions
//

#if defined (_DEBUG)
VOID DebugOut(LPCTSTR pszLabel, const unsigned char* ptr, size_t cb) {

	OutputDebugString( pszLabel );
	OutputDebugString( TEXT( ": " ) );
	for (decltype(cb) i = 0; i < cb; ++i){
		int n = static_cast<int>( *(ptr + i) );

		const size_t cchBuf = 128;
		TCHAR szBuf[cchBuf] = { 0 };
		StringCchPrintf( szBuf, cchBuf, TEXT( "%02x" ), n);
		OutputDebugString( szBuf );
	}
	OutputDebugString( TEXT( "\x0A" ) );
}

std::unique_ptr<xor_t> get_vex_xor_impl(void);

class dbg_xor_t : public xor_t {
public:
	typedef std::unique_ptr<xor_t> delegate_type;

	dbg_xor_t(delegate_type&& ptr): m_delegate( std::move( ptr ) ) { }

	size_type apply(operand_type front, operand_type back, size_type cb) const {

		OutputDebugString( TEXT( "Scalar: " ) );
		for (decltype(cb) i = 0; i < cb; ++i){
			int n = static_cast<int>( *(front + i) ^ *(back + i) );

			const size_t cchBuf = 128;
			TCHAR szBuf[cchBuf] = { 0 };
			StringCchPrintf( szBuf, cchBuf, TEXT( "%02x" ), n);
			OutputDebugString( szBuf );
		}
		OutputDebugString( TEXT( "\x0A" ) );

		const auto result = m_delegate->apply( front, back, cb );

		const size_t cchLabel = 16;
		TCHAR szLabel[cchLabel] = { 0 };
		::StringCchPrintf( szLabel, cchLabel, TEXT( "Delegate (%d)" ), static_cast<int>( m_delegate->vex( ) ) );
		DebugOut( szLabel, front, cb );
		return result;
	}

	XORVex vex() const {
		return m_delegate->vex( );
	}

private:
	delegate_type m_delegate;
};

std::unique_ptr<xor_t> get_vex_xor(void) {
	return std::make_unique<dbg_xor_t>( get_vex_xor_impl( ) );
}
#else
#define get_vex_xor_impl get_vex_xor
#endif // defined (_DEBUG)

// Given the output of CPUID, returns the name of the CPU vendor in a string
std::string get_cpu_vendor(int cpuid[]) {

	std::string name;
	std::array<int, 3> indices = { 1, 3, 2 };
	std::for_each( indices.cbegin( ), indices.cend( ), [&](int offset) {
		auto ptr = reinterpret_cast<char*>( cpuid + offset );
		for (auto i = 0; i < sizeof( int ); ++i){
			name.append( 1, *(ptr + i) );
		}
	} );
	return name;
}

std::unique_ptr<xor_t> get_vex_xor_impl(void) {

#if defined(_M_ARM64)
	return std::make_unique<neon_xor_t>( );
#else
	// Get the CPU ID
	int info[4] = { -1, -1, -1, -1 };
	__cpuid(info, 0);

	// Check if we are on supported hardware
	const std::set<std::string> vendors = { "GenuineIntel", "AuthenticAMD" };
	const auto vendor = get_cpu_vendor( info );
	if (vendors.count( vendor ) > 0){
		// Query for the feature flags
		__cpuid( info, 1 );

		// Is AVX supported? (c.f. http://software.intel.com/en-us/blogs/2011/04/14/is-avx-enabled/)
		const bool os_uses_XSAVE = (info[2] & (1 << 27)) != 0;
		const bool cpu_supports_AVX = (info[2] & (1 << 28)) != 0;
		if (os_uses_XSAVE && cpu_supports_AVX){
			// Check if the OS will save the YMM registers
			unsigned long long xcr_feature_mask = _xgetbv( _XCR_XFEATURE_ENABLED_MASK );
			if ((xcr_feature_mask & 0x6) != 0){
				// AVX: good to G.O.
				return std::make_unique<avx_xor_t>( );
			}
		}

		// How about SSE2?
		if ((info[3] & (1 << 26)) != 0){
			return std::make_unique<sse2_xor_t>( );
		}

		// SSE?
		if ((info[3] & (1 << 25)) != 0){
			return std::make_unique<sse_xor_t>( );
		}

#if !defined (_WIN64)
		// MMX?
		if ((info[3] & (1 << 23)) != 0){
			return std::make_unique<mmx_xor_t>( );
		}
#endif // _WIN64
	}
#endif // !defined(_M_ARM64)

	// If we get here, just return the default implementation
	return std::make_unique<xor_t>( );
}
