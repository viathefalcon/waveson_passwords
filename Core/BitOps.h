// BitOps.h: defines classes, etc., for operating on individual bits
//			 or collections thereof
//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

#if !defined(__BITOPS_H__)
#define __BITOPS_H__


// Includes
//

// C++ Standard Library Headers
#include <string>
#include <vector>
#include <memory>
#include <utility>

// C Standard Library Headers
#include <limits.h>

// Local Project Headers
#include "exports.h"

// Types
//

// Enumerates the vector extensions with which
// we accelerate the application of XOR to buffers
// of bytes
typedef enum _XORVex {

	XORVexNONE = 0,
	XORVexMMX = 1,
	XORVexSSE = 2,
	XORVexSSE2 = 4,
	XORVexAVX = 8,
    XORVexNEON = 16

} XORVex;

// Templates
//

/// <returns>A string of the values of individual bits in the given
/// value, most significant bit to least significant bit, left to right,
/// grouped into nibbles of 4 bits</returns>
template <typename T>
std::string bits_to_string(T u)
{
    std::string str;

    CHAR szBuf[128] = { 0 };
    const auto hr = StringCchPrintfA(szBuf, sizeof(szBuf), "%llu == ", u);
    if (SUCCEEDED( hr ))
    {
        str = decltype(str)(szBuf);

        for (decltype(u) mask = decltype(u){1} << (sizeof(decltype(u)) * CHAR_BIT - 1), counter = 0; mask; mask >>= 1, ++counter)
        {
            if ((counter > 0) && ((counter % 4) == 0))
            {
                str.push_back(' ');
            }

            if (u & mask)
            {
                str.push_back('1');
            }
            else
            {
                str.push_back('0');
            }
        }
    }

    return str;
}

/// <returns>The minium number of bits needed to represent numbers up to and including a given size</returns>
template <typename size_type>
static size_type min_bit_count(size_type s) {
    size_type bits = 0;
    while ((static_cast<size_type>(1) << bits) < s)
    {
        bits++;
    }
    return bits;
}

/// <returns>A value that will mask off up to and including the given number of least significant bits</returns>
template <typename value_type, typename size_type>
static value_type generate_bit_mask(size_type bits) {
    value_type mask = 0;
    for (decltype(bits) remaining = 0; remaining < bits; ++remaining)
    {
        mask |= (static_cast<decltype(remaining)>(1) << remaining);
    }
    return mask;
}

template <typename T = uintmax_t>
class selector_t {
public:
    typedef T value_type;
    typedef size_t size_type;

    selector_t(const value_type& value, size_type bound):
        m_value( value ),
        m_offset( 0 ),
        m_bound( bound ),
        m_bit_count( min_bit_count( bound ) ),
        m_bit_mask( generate_bit_mask<T, size_type>( m_bit_count ) )
    { }

    inline bool has_next(void) const {
        return remaining( ) >= m_bit_count;
    }

    value_type next(void) {

        // Loop until we find a bit string which, when masked,
        // selects a value less than the bound, or run out of bits
        while (has_next( )){
            const auto next = m_value >> m_offset;
            const auto masked = (next & m_bit_mask);
            if (masked < m_bound)
            {
                // Happy days
                m_offset += m_bit_count;
                return masked;
            }

            // Need to go again, dropping the least significant bit
            ++m_offset;
        }

        // We didn't find a suitable value so return the provided
        // upper bound as an error signal
        return m_bound;
    }

    void reset(const value_type& value) {
        m_value = value;
        m_offset = 0;
    }

private:
    /// <returns>The number of bits in the value which haven't been used yet</returns>
    inline size_type remaining(void) const {
        return (bits_per_value - m_offset);
    }

    value_type m_value;
    size_type m_offset;
    const size_type m_bound, m_bit_count, m_bit_mask;

    static const size_type bits_per_value = (CHAR_BIT * sizeof( value_type ));
};

// Classes
//

// Defines the interface for a class for tracking the state of
// variable-sized sets of individual bits
class empty_bitset_t {
public:
    empty_bitset_t(size_t size): m_size( size ) { }
    virtual ~empty_bitset_t() {
#if defined (_DEBUG)
		OutputDebugString( TEXT( "Destroying empty_bitset_t...\x0A" ) );
#endif
    }

    virtual void set(size_t bit) {
        // Do nothing
        ;
    }

    virtual bool is_set(size_t bit) const {
        return false;
    }

    virtual void reset() { }

    size_t size(void) const {
        return m_size;
    }

private:
    size_t m_size;
};

// Defines the implementation for a class for tracking the state of
// variable-sized sets of individual bits
class bitset_t : public empty_bitset_t {
public:
    bitset_t(size_t size):
        empty_bitset_t( size ),
        m_words( word_count( size ), zero_word ) { }
    virtual ~bitset_t() {
#if defined (_DEBUG)
		OutputDebugString( TEXT( "Destroying bitset_t...\x0A" ) );
#endif
    }

    void set(size_t bit) override {
        auto got = this->at( bit );
        m_words[got.first] |= got.second;
    }

    bool is_set(size_t bit) const override {
        const auto got = this->at( bit );
        return ((m_words[got.first] & got.second) != 0);
    }

    void reset() override {        
        for (auto it = m_words.begin( ), end = m_words.end( ); it != end; ++it){
            (*it) = zero_word;
        }
    }

private:
    static size_t word_count(size_t size) {

        const auto wc = size / bits_per_word;
        return ((size % bits_per_word) == 0)
            ? wc
            : (wc + 1);
    }

    typedef unsigned int word_type;
    typedef ::std::vector<word_type> word_string;

    std::pair<size_t, word_type> at(size_t index) const {

        // Find the word
        const auto word = (index / bits_per_word);

        // Generate the mask
        const auto bit = (index % bits_per_word);
        const word_type mask = (word_type{1} << bit);

        // Return
        return std::make_pair( word, mask );
    }

    static const word_type zero_word = 0;
    static const size_t bits_per_word = (CHAR_BIT * sizeof( word_type ));

    word_string m_words;
};

class xor_t {
public:
	typedef size_t size_type;
	typedef unsigned char* operand_type;

    virtual ~xor_t() = default;

	virtual size_type apply(operand_type front, operand_type back, size_type cb) const {

		for (decltype(cb) i = 0; i < cb; ++i){
			*(front + i) ^= *(back + i);
		}
		return cb;
	}

	virtual XORVex vex() const {
		return XORVexNONE;
	}
};

// Functions
//

// Returns an object which can be used to apply Exclusive-OR to pairs
// of byte buffers using the widest-available vector extensions
WPG_CORE_API std::unique_ptr<xor_t> get_vex_xor(void);

#endif // __BITOPS_H__
