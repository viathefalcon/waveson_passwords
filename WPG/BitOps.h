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
#include <memory>

// C Standard Library Headers
#include <limits.h>

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

    void set(size_t bit) {
        apply(
            [this](size_t word, const word_type& mask) -> void {
                const auto masked = (m_words[word] | mask);
                m_words.replace( word, 1, 1, masked );
            },
            bit
        );
    }

    bool is_set(size_t bit) const {

        bool result = false;
        apply(
            [this, &result](size_t word, const word_type& mask) -> void {
                result = ((m_words[word] & mask) != 0);
            },
            bit
        );
        return result;
    }

    void reset() {
        
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

    template<typename Lambda>
    void apply(Lambda lambda, size_t index) const {

        // Find the word
        const auto word = (index / bits_per_word);

        // Generate the mask
        const auto bit = (index % bits_per_word);
        const word_type mask = (1 << bit);
        lambda( word, mask );
    }

    typedef unsigned int word_type;
    typedef ::std::basic_string<word_type> word_string;

    static const word_type zero_word = 0;
    static const size_t bits_per_word = (CHAR_BIT * sizeof( word_type ));

    word_string m_words;
};

class xor_t {
public:
	typedef size_t size_type;
	typedef unsigned char* operand_type;

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
std::unique_ptr<xor_t> get_vex_xor(void);

#endif // __BITOPS_H__
