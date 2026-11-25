#include "ImText.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace imtext
{
namespace
{
const FontChoice kFonts[] = {
	{ "Arial Black", "ariblk.ttf" },
	{ "Impact", "impact.ttf" },
	{ "Segoe UI Black", "seguibl.ttf" },
	{ "Bahnschrift", "bahnschrift.ttf" },
	{ "Arial Bold", "arialbd.ttf" },
};

struct LoadedFont
{
	std::vector< unsigned char > data;
	stbtt_fontinfo info;
	bool ok = false;
};

std::mutex g_mutex;
std::map< std::string, std::shared_ptr< LoadedFont > > g_cache;

std::string FontDir()
{
#if defined( _WIN32 )
	char buf[ MAX_PATH ] = { 0 };
	UINT n               = GetWindowsDirectoryA( buf, MAX_PATH );
	std::string dir      = n ? std::string( buf, n ) : std::string( "C:\\Windows" );
	return dir + "\\Fonts\\";
#else
	return "";
#endif
}

std::shared_ptr< LoadedFont > LoadPath( const std::string& path )
{
	auto it = g_cache.find( path );
	if( it != g_cache.end() )
		return it->second;
	auto font = std::make_shared< LoadedFont >();
	if( FILE* f = std::fopen( path.c_str(), "rb" ) )
	{
		std::fseek( f, 0, SEEK_END );
		long size = std::ftell( f );
		std::fseek( f, 0, SEEK_SET );
		if( size > 0 )
		{
			font->data.resize( (size_t)size );
			if( std::fread( font->data.data(), 1, (size_t)size, f ) == (size_t)size )
			{
				int offset = stbtt_GetFontOffsetForIndex( font->data.data(), 0 );
				font->ok   = offset >= 0 && stbtt_InitFont( &font->info, font->data.data(), offset ) != 0;
			}
		}
		std::fclose( f );
	}
	g_cache[ path ] = font;
	return font;
}

// Tries the requested Windows font, then sensible fallbacks, then common Linux fonts (for the test host).
std::shared_ptr< LoadedFont > LoadFont( const char* preferredFile, bool mono )
{
	std::vector< std::string > candidates;
	std::string dir = FontDir();
	if( preferredFile )
		candidates.push_back( dir + preferredFile );
	if( mono )
	{
		candidates.push_back( dir + "consola.ttf" );
		candidates.push_back( dir + "cour.ttf" );
	}
	candidates.push_back( dir + "arialbd.ttf" );
	candidates.push_back( dir + "arial.ttf" );
	if( mono )
		candidates.push_back( "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf" );
	candidates.push_back( "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" );
	candidates.push_back( "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf" );
	for( auto& c : candidates )
	{
		auto f = LoadPath( c );
		if( f->ok )
			return f;
	}
	return nullptr;
}

// Minimal UTF-8 decoder, enough for words typed into Arena.
std::vector< int > Codepoints( const std::string& s )
{
	std::vector< int > out;
	for( size_t i = 0; i < s.size(); )
	{
		unsigned char c = (unsigned char)s[ i ];
		int cp = '?', len = 1;
		if( c < 0x80 )
			cp = c;
		else if( ( c >> 5 ) == 6 && i + 1 < s.size() )
			cp = ( ( c & 0x1f ) << 6 ) | ( s[ i + 1 ] & 0x3f ), len = 2;
		else if( ( c >> 4 ) == 14 && i + 2 < s.size() )
			cp = ( ( c & 0x0f ) << 12 ) | ( ( s[ i + 1 ] & 0x3f ) << 6 ) | ( s[ i + 2 ] & 0x3f ), len = 3;
		else if( ( c >> 3 ) == 30 && i + 3 < s.size() )
			cp = ( ( c & 0x07 ) << 18 ) | ( ( s[ i + 1 ] & 0x3f ) << 12 ) | ( ( s[ i + 2 ] & 0x3f ) << 6 ) | ( s[ i + 3 ] & 0x3f ), len = 4;
		if( cp >= 'a' && cp <= 'z' )
			cp -= 32;
		out.push_back( cp );
		i += len;
	}
	return out;
}

// Draws one glyph with max-blending into a top-down image.
void Blit( std::vector< unsigned char >& img, int W, int H, const stbtt_fontinfo& info, float scale, int cp, float x, float baseline )
{
	int x0, y0, x1, y1;
	float sx = x - std::floor( x );
	stbtt_GetCodepointBitmapBoxSubpixel( &info, cp, scale, scale, sx, 0, &x0, &y0, &x1, &y1 );
	int gw = x1 - x0, gh = y1 - y0;
	if( gw <= 0 || gh <= 0 )
		return;
	std::vector< unsigned char > g( (size_t)gw * gh );
	stbtt_MakeCodepointBitmapSubpixel( &info, g.data(), gw, gh, gw, scale, scale, sx, 0, cp );
	int ox = (int)std::floor( x ) + x0, oy = (int)std::floor( baseline ) + y0;
	for( int j = 0; j < gh; ++j )
	{
		int py = oy + j;
		if( py < 0 || py >= H )
			continue;
		for( int i = 0; i < gw; ++i )
		{
			int px = ox + i;
			if( px < 0 || px >= W )
				continue;
			unsigned char& d = img[ (size_t)py * W + px ];
			d                = std::max( d, g[ (size_t)j * gw + i ] );
		}
	}
}

void FlipRows( std::vector< unsigned char >& img, int W, int H )
{
	for( int j = 0; j < H / 2; ++j )
		std::swap_ranges( img.begin() + (size_t)j * W, img.begin() + (size_t)( j + 1 ) * W, img.begin() + (size_t)( H - 1 - j ) * W );
}
}// namespace

const FontChoice* FontChoices( int* count )
{
	*count = (int)( sizeof( kFonts ) / sizeof( kFonts[ 0 ] ) );
	return kFonts;
}

bool RenderWord( const std::string& word, int fontIndex, std::vector< unsigned char >& out )
{
	std::lock_guard< std::mutex > lock( g_mutex );
	out.assign( (size_t)kWordW * kWordH, 0 );
	int n            = 0;
	const auto* list = FontChoices( &n );
	fontIndex        = std::max( 0, std::min( n - 1, fontIndex ) );
	auto font        = LoadFont( list[ fontIndex ].file, false );
	if( !font )
		return false;
	const stbtt_fontinfo& info = font->info;
	std::vector< int > cps     = Codepoints( word.empty() ? std::string( " " ) : word );

	int ascent, descent, lineGap;
	stbtt_GetFontVMetrics( &info, &ascent, &descent, &lineGap );
	float scale = stbtt_ScaleForPixelHeight( &info, 400.0f );
	// measure
	float width = 0;
	for( size_t i = 0; i < cps.size(); ++i )
	{
		int adv, lsb;
		stbtt_GetCodepointHMetrics( &info, cps[ i ], &adv, &lsb );
		width += adv;
		if( i + 1 < cps.size() )
			width += stbtt_GetCodepointKernAdvance( &info, cps[ i ], cps[ i + 1 ] );
	}
	float maxW = 1880.0f;
	if( width * scale > maxW )
		scale = maxW / width;
	float textH    = ( ascent - descent ) * scale;
	float baseline = ( kWordH - textH ) * 0.5f + ascent * scale + 8.0f;
	float x        = ( kWordW - width * scale ) * 0.5f;
	for( size_t i = 0; i < cps.size(); ++i )
	{
		Blit( out, kWordW, kWordH, info, scale, cps[ i ], x, baseline );
		int adv, lsb;
		stbtt_GetCodepointHMetrics( &info, cps[ i ], &adv, &lsb );
		x += adv * scale;
		if( i + 1 < cps.size() )
			x += stbtt_GetCodepointKernAdvance( &info, cps[ i ], cps[ i + 1 ] ) * scale;
	}
	FlipRows( out, kWordW, kWordH );
	return true;
}

bool RenderGlyphAtlas( std::vector< unsigned char >& out )
{
	std::lock_guard< std::mutex > lock( g_mutex );
	out.assign( (size_t)kGlyphW * kGlyphH, 0 );
	auto font = LoadFont( "consola.ttf", true );
	if( !font )
		return false;
	const stbtt_fontinfo& info = font->info;
	const char* ramp           = " .,:-=+*oxO0#%@";
	float scale                = stbtt_ScaleForPixelHeight( &info, 50.0f );
	int ascent, descent, lineGap;
	stbtt_GetFontVMetrics( &info, &ascent, &descent, &lineGap );
	float baseline = ( kGlyphH - ( ascent - descent ) * scale ) * 0.5f + ascent * scale;
	for( int i = 0; i < 15; ++i )
	{
		int cp = ramp[ i ];
		int adv, lsb;
		stbtt_GetCodepointHMetrics( &info, cp, &adv, &lsb );
		float x = i * 40.0f + ( 40.0f - adv * scale ) * 0.5f;
		Blit( out, kGlyphW, kGlyphH, info, scale, cp, x, baseline );
	}
	for( int y = 6; y < 58; ++y )// solid block for the brightest cells
		for( int x = 15 * 40 + 3; x < 15 * 40 + 37; ++x )
			out[ (size_t)y * kGlyphW + x ] = 255;
	FlipRows( out, kGlyphW, kGlyphH );
	return true;
}
}// namespace imtext
