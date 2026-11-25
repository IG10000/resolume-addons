#pragma once
#include <string>
#include <vector>

// CPU text rasterising with stb_truetype, using fonts already installed on the computer.
namespace imtext
{
constexpr int kWordW  = 2048;
constexpr int kWordH  = 512;
constexpr int kGlyphW = 640;// 16 glyphs of 40 x 64
constexpr int kGlyphH = 64;

struct FontChoice
{
	const char* label;
	const char* file;// file name inside the Windows Fonts folder
};
const FontChoice* FontChoices( int* count );

// Draws `word` (upper-cased, centred, as big as fits) into a kWordW x kWordH 8-bit image.
// Row 0 of the result is the BOTTOM of the image, ready for glTexImage2D. Returns false if no font loads.
bool RenderWord( const std::string& word, int fontIndex, std::vector< unsigned char >& out );

// Draws the 16 glyph ASCII ramp " .,:-=+*oxO0#%@" plus a solid block into a kGlyphW x kGlyphH image.
bool RenderGlyphAtlas( std::vector< unsigned char >& out );
}// namespace imtext
