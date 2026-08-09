//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

#include "pch.h"

#include <BitOps.h>
#include <WPGGenerators.h>

namespace
{
	/// <summary>
	/// Writes, to the logger, the values of individual bits in the given
	/// value, most significant bit to least significant bit, left to right,
	/// grouped into nibbles of 4 bits
	/// </summary>
	template <typename T>
	void LogBits(T u)
	{
		const auto str = bits_to_string(u);
		Logger::WriteMessage(str.c_str());
	}

	/// <summary>
	/// Mocks a finite, deterministic "random" number generator
	/// </summary>
	class test_rng_t final : public rng_t {
	public:
		typedef unsigned char value_type;
		typedef ::std::vector<value_type> container_type;

		explicit test_rng_t(container_type&& bytes):
			m_bytes( std::move( bytes ) ),
			m_read( 0U ) { }

		virtual ~test_rng_t() = default;

		operator WPGCap() const override { return WPGCapTest; }
		operator bool() const override { return true; }

		size_type fill(void* dst, size_type size) override {

			const auto available = m_bytes.size( ) - m_read;
			const auto n = static_cast<size_type>(min(static_cast<decltype(m_bytes)::size_type>( size ), available));
			if (n > 0){
				CopyMemory( dst, m_bytes.data( ) + m_read, n );
			}

			m_read += static_cast<decltype(m_read)>( n );
			return n;
		}

	private:
		container_type m_bytes;
		decltype(m_bytes)::size_type m_read;
	};

	class test_wpg_t final : public wpg_t {
	public:
		test_wpg_t(::std::unique_ptr<rng_t> rng): wpg_t( test_rngs( ::std::move( rng ) ), get_vex_xor( ) ) { }
		virtual ~test_wpg_t() = default;

	private:
		static ::std::vector<::std::unique_ptr<rng_t>> test_rngs(::std::unique_ptr<rng_t> rng) {
			::std::vector<::std::unique_ptr<rng_t>> v;
			v.emplace_back(::std::move(rng));
			return v;
		}
	};
}

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
	TEST_CLASS(WPGGenerator)
	{
	public:
		TEST_METHOD_INITIALIZE(Initialise)
		{
			auto hr = StringCchLength( pszAlphabet, STRSAFE_MAX_CCH, &cchAlphabet) ;
			Assert::IsTrue( SUCCEEDED( hr ) );
		}

		TEST_METHOD(TestSelectorNoOverflow)
		{
			// Arrange: construct an input
			const size_t bits = 7;
			selector_t<>::value_type value = 0;
			std::vector<selector_t<>::value_type> indices = { 14, 8, 9, 23, 61, 44, 68 };
			for (decltype(indices)::size_type i = 0; i < indices.size( ); ++i)
			{
				value |= (indices[i] << (bits * i));
			}
			LogBits(value);

			// Act, assert
			selector_t<> fixture( value, cchAlphabet );
			for (size_t counter = 0; fixture.has_next( ) && (counter < indices.size( )); ++counter)
			{
				const auto next = fixture.next();
				Assert::AreEqual(indices[counter], next);
			}
		}

		TEST_METHOD(TestSelectorMultipleOverflow)
		{
			// Arrange: construct an input
			const size_t bits = 7;
			selector_t<>::value_type value = 0;
			std::vector<selector_t<>::value_type> indices = { 14, 8, 9, 75, 61, 44, 69 };
			for (decltype(indices)::size_type i = 0; i < indices.size( ); ++i)
			{
				value |= (indices[i] << (bits * i));
			}
			LogBits(value);

			// Act, assert
			selector_t<> fixture( value, cchAlphabet );
			for (size_t counter = 0; fixture.has_next( ) && (counter < indices.size( )); ++counter)
			{
				const auto next = fixture.next();
				Assert::IsTrue(next < cchAlphabet);
			}
		}

		TEST_METHOD(TestWpgGenerate)
		{
			// Arrange
			const ::test_rng_t::container_type indices = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
			auto rng = ::std::make_unique<test_rng_t>(
				make_rng_values( indices, min_bit_count( cchAlphabet ) )
			);
			const size_t cchBuffer = 128;
			TCHAR szBuffer[cchBuffer] = { 0 };
			BYTE cchGenerated = 0;

			// Act
			test_wpg_t fixture( ::std::move( rng ) );
			const auto failed = fixture.Generate(
				szBuffer, 
				static_cast<BYTE>( indices.size( ) ), 
				WPGCapTest, 
				&cchGenerated,
				pszAlphabet,
				false );

			// Assert
			Assert::AreEqual( static_cast<WPGCaps>( WPGCapNONE ), failed );
			Assert::AreEqual( indices.size( ), static_cast<decltype(indices)::size_type>( cchGenerated ) );
			for (auto i = 0; i < indices.size( ); ++i)
			{
				Assert::AreEqual( pszAlphabet[indices.at( i )], szBuffer[i] );
			}

			// Output
			Logger::WriteMessage( "Generated: " );
			Logger::WriteMessage( szBuffer );
			Logger::WriteMessage( "\n" );
		}

		TEST_METHOD(TestWpgGenerateRNGFailure)
		{
			// Arrange
			::test_rng_t::container_type values = { };
			auto rng = ::std::make_unique<test_rng_t>( std::move( values ) );
			const size_t cchBuffer = 128;
			TCHAR szBuffer[cchBuffer] = { 0 };
			BYTE cchGenerated = 0;
			const auto cap = WPGCapTest;

			// Act
			test_wpg_t fixture( ::std::move( rng ) );
			const auto failed = fixture.Generate(
				szBuffer, 
				static_cast<BYTE>( cchBuffer ), 
				cap, 
				&cchGenerated,
				pszAlphabet,
				false );

			// Assert
			Assert::AreEqual( static_cast<WPGCaps>( cap ), failed );
			Assert::AreEqual( 0, static_cast<int>( cchGenerated ) );
		}

		TEST_METHOD(TestWpgGenerateCapsNONE)
		{
			// Arrange
			const ::test_rng_t::container_type indices = { 0, 1, 2, 3, 4 };
			auto rng = ::std::make_unique<test_rng_t>(
				make_rng_values( indices, min_bit_count( cchAlphabet ) )
			);
			const size_t cchBuffer = 128;
			TCHAR szBuffer[cchBuffer] = { 0 };
			BYTE cchGenerated = 0;

			// Act
			test_wpg_t fixture( ::std::move( rng ) );
			const auto failed = fixture.Generate(
				szBuffer, 
				static_cast<BYTE>( indices.size( ) ), 
				WPGCapNONE, 
				&cchGenerated,
				pszAlphabet,
				false );

			// Assert
			Assert::AreEqual( 0, static_cast<int>( cchGenerated ) );
		}

	private:
		LPCTSTR pszAlphabet = TEXT("abcdefghijklmnopqrstuvwxyz1234567890!@#$%&*ABCDEFGHIJKLMNOPQRSTUVWXYZ");
		size_t cchAlphabet;

		/// <summary>Packs the given values into a container along the given bit boundary</summary>
		::test_rng_t::container_type make_rng_values(
			::test_rng_t::container_type values,
			::std::size_t bits_per_value)
		{
			::test_rng_t::container_type result;
			auto accumulate_word = [&result](size_t word) -> void
			{
				Logger::WriteMessage( "Accumulating: " );
				LogBits( word );
				Logger::WriteMessage( "\n" );

				for (size_t i = 0; i < sizeof( size_t ); ++i)
				{
					// Get the current value by masking off the least significant byte,
					// and accumulate it
					const auto value = static_cast<::test_rng_t::value_type>(word & UCHAR_MAX);
					result.push_back( value );

					// Bring the next byte into view
					word >>= CHAR_BIT;
				}
			};

			if (values.empty( ))
			{
				// Nothing to do!
				Logger::WriteMessage( "make_rng_values called without any values!\n" );
			}
			else
			{
				// Generate the mask to apply to each value
				const auto mask = generate_bit_mask<size_t, size_t>( bits_per_value );
				Logger::WriteMessage( "Generated mask: " );
				LogBits( mask );
				Logger::WriteMessage( "\n" );
				
				// Assemble each word to be accumulated, from the least significant byte up
				size_t word = 0, counter = 0;
				for (const auto value : values)
				{
					const auto masked = static_cast<decltype(word)>(value) & mask;
					Logger::WriteMessage( "Masked: " );
					LogBits( masked );
					Logger::WriteMessage( "\n" );

					word |= (masked << (bits_per_value * counter));
					Logger::WriteMessage( "Appended: " );
					LogBits( word );
					Logger::WriteMessage( "\n" );

					// Advance & check bounds
					if (++counter >= sizeof( word ))
					{
						accumulate_word( word );
						word = counter = 0;
					}
				}

				// If a word was partially under construction when the preceding
				// loop terminated, then accumulate it
				if (counter)
				{
					accumulate_word( word );
				}
			}

			return result;
		}
	};
}
