#include "ImLicense.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#if defined( _WIN32 )
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace imlic
{
namespace
{
// Fingerprint = SHA-256( key ), where key = SHA-256( "IMMINENCE|" + unlock code ) as lowercase hex.
// The key is what gets written to the key file; the code itself is never stored.
const char* kKeyFingerprint = "50ff5b8d5d16bdb05d14f3a4ce59fcaf2013ce9c998c2e2e9c9b13f0d6618fe7";
const char* kSalt           = "IMMINENCE|";

std::mutex g_mutex;
bool g_unlocked = false;
bool g_checkedOnce = false;
std::chrono::steady_clock::time_point g_lastCheck;

const uint32_t K[ 64 ] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

inline uint32_t rotr( uint32_t x, uint32_t n )
{
	return ( x >> n ) | ( x << ( 32 - n ) );
}

std::string Trim( const std::string& s )
{
	size_t a = s.find_first_not_of( " \t\r\n" );
	if( a == std::string::npos )
		return std::string();
	size_t b = s.find_last_not_of( " \t\r\n" );
	return s.substr( a, b - a + 1 );
}

void MakeDirs( const std::string& path )
{
	for( size_t i = 1; i < path.size(); ++i )
	{
		if( path[ i ] == '/' || path[ i ] == '\\' )
		{
			std::string part = path.substr( 0, i );
#if defined( _WIN32 )
			_mkdir( part.c_str() );
#else
			mkdir( part.c_str(), 0755 );
#endif
		}
	}
}

std::string ReadKeyFile()
{
	std::string path = KeyFilePath();
	FILE* f          = std::fopen( path.c_str(), "rb" );
	if( !f )
		return std::string();
	char buf[ 256 ];
	size_t n = std::fread( buf, 1, sizeof( buf ) - 1, f );
	std::fclose( f );
	buf[ n ] = 0;
	return Trim( buf );
}
}// namespace

std::string Sha256Hex( const std::string& data )
{
	uint32_t h[ 8 ] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
	std::string msg = data;
	uint64_t bitLen = (uint64_t)data.size() * 8;
	msg.push_back( (char)0x80 );
	while( msg.size() % 64 != 56 )
		msg.push_back( 0 );
	for( int i = 7; i >= 0; --i )
		msg.push_back( (char)( ( bitLen >> ( i * 8 ) ) & 0xff ) );

	for( size_t off = 0; off < msg.size(); off += 64 )
	{
		uint32_t w[ 64 ];
		for( int i = 0; i < 16; ++i )
		{
			const unsigned char* p = (const unsigned char*)msg.data() + off + i * 4;
			w[ i ]                 = ( (uint32_t)p[ 0 ] << 24 ) | ( (uint32_t)p[ 1 ] << 16 ) | ( (uint32_t)p[ 2 ] << 8 ) | p[ 3 ];
		}
		for( int i = 16; i < 64; ++i )
		{
			uint32_t s0 = rotr( w[ i - 15 ], 7 ) ^ rotr( w[ i - 15 ], 18 ) ^ ( w[ i - 15 ] >> 3 );
			uint32_t s1 = rotr( w[ i - 2 ], 17 ) ^ rotr( w[ i - 2 ], 19 ) ^ ( w[ i - 2 ] >> 10 );
			w[ i ]      = w[ i - 16 ] + s0 + w[ i - 7 ] + s1;
		}
		uint32_t a = h[ 0 ], b = h[ 1 ], c = h[ 2 ], d = h[ 3 ], e = h[ 4 ], f = h[ 5 ], g = h[ 6 ], hh = h[ 7 ];
		for( int i = 0; i < 64; ++i )
		{
			uint32_t S1  = rotr( e, 6 ) ^ rotr( e, 11 ) ^ rotr( e, 25 );
			uint32_t ch  = ( e & f ) ^ ( ~e & g );
			uint32_t t1  = hh + S1 + ch + K[ i ] + w[ i ];
			uint32_t S0  = rotr( a, 2 ) ^ rotr( a, 13 ) ^ rotr( a, 22 );
			uint32_t maj = ( a & b ) ^ ( a & c ) ^ ( b & c );
			uint32_t t2  = S0 + maj;
			hh           = g;
			g            = f;
			f            = e;
			e            = d + t1;
			d            = c;
			c            = b;
			b            = a;
			a            = t1 + t2;
		}
		h[ 0 ] += a;
		h[ 1 ] += b;
		h[ 2 ] += c;
		h[ 3 ] += d;
		h[ 4 ] += e;
		h[ 5 ] += f;
		h[ 6 ] += g;
		h[ 7 ] += hh;
	}
	static const char* hex = "0123456789abcdef";
	std::string out;
	out.reserve( 64 );
	for( int i = 0; i < 8; ++i )
		for( int j = 28; j >= 0; j -= 4 )
			out.push_back( hex[ ( h[ i ] >> j ) & 0xf ] );
	return out;
}

std::string KeyFilePath()
{
#if defined( _WIN32 )
	const char* base = std::getenv( "APPDATA" );
	std::string root = base ? base : "C:\\";
	return root + "\\IMMINENCE\\unlock.key";
#else
	const char* override = std::getenv( "IMMINENCE_KEY_FILE" );
	if( override && *override )
		return override;
	const char* base = std::getenv( "HOME" );
	std::string root = base ? base : "/tmp";
	return root + "/.config/IMMINENCE/unlock.key";
#endif
}

bool IsUnlocked()
{
	std::lock_guard< std::mutex > lock( g_mutex );
	if( g_unlocked )
		return true;
	auto now = std::chrono::steady_clock::now();
	if( g_checkedOnce && now - g_lastCheck < std::chrono::seconds( 2 ) )
		return false;
	g_checkedOnce = true;
	g_lastCheck   = now;
	std::string key = ReadKeyFile();
	if( !key.empty() && Sha256Hex( key ) == kKeyFingerprint )
		g_unlocked = true;
	return g_unlocked;
}

bool TryUnlock( const std::string& code )
{
	std::string typed = Trim( code );
	if( typed.empty() )
		return false;
	std::string key = Sha256Hex( std::string( kSalt ) + typed );
	if( Sha256Hex( key ) != kKeyFingerprint )
		return false;

	std::string path = KeyFilePath();
	MakeDirs( path );
	if( FILE* f = std::fopen( path.c_str(), "wb" ) )
	{
		std::fwrite( key.data(), 1, key.size(), f );
		std::fclose( f );
	}
	std::lock_guard< std::mutex > lock( g_mutex );
	g_unlocked = true;
	return true;
}
}// namespace imlic
