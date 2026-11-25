#include "ImPlugin.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "ImLicense.h"
#include "ImShaders.h"
#include "ImText.h"
#include "generated/prelude.inc"

using namespace ffglex;

namespace
{
const char* kQualityLabels[] = { "High", "Medium", "Low" };
const float kQualityScale[]  = { 1.0f, 0.75f, 0.5f };
const char* kDefaultWord     = "IMMINENCE";
const int kHistFrames        = 48;
const int kMaxUnits          = 8;
}// namespace

GLint ImPlugin::Program::Loc( const char* name )
{
	auto it = locs.find( name );
	if( it != locs.end() )
		return it->second;
	GLint l       = glGetUniformLocation( id, name );
	locs[ name ] = l;
	return l;
}

ImPlugin::ImPlugin( const ImFamilyDef& fam ) :
	CFFGLPlugin( false ),
	family( fam )
{
	startTime = lastFrame = std::chrono::steady_clock::now();

	switch( family.kind )
	{
	case IM_KIND_EFFECT:
		SetMinInputs( 1 );
		SetMaxInputs( 1 );
		break;
	case IM_KIND_SOURCE:
		SetMinInputs( 0 );
		SetMaxInputs( 0 );
		break;
	case IM_KIND_MIXER:
		SetMinInputs( 2 );
		SetMaxInputs( 2 );
		break;
	}

	for( int s = 0; s < family.numStyles; ++s )
		if( family.styles[ s ].flags & IM_TEXT )
			needsText = true;

	if( family.kind == IM_KIND_MIXER )
		progressParam = AddHostParam( { R_PROGRESS } );
	if( family.numStyles > 1 )
		styleParam = AddHostParam( { R_STYLE } );
	for( int s = 0; s < family.numStyles; ++s )
		for( int p = 0; p < family.styles[ s ].numParams; ++p )
		{
			HostParam hp;
			hp.role  = R_STYLE_PARAM;
			hp.style = s;
			hp.def   = &family.styles[ s ].params[ p ];
			AddHostParam( hp );
		}
	if( needsText )
	{
		wordParam = AddHostParam( { R_WORD } );
		fontParam = AddHostParam( { R_FONT } );
	}
	if( family.kind != IM_KIND_MIXER )
		beatParam = AddHostParam( { R_BEAT } );
	if( family.kind == IM_KIND_EFFECT )
		mixParam = AddHostParam( { R_MIX } );
	qualityParam = AddHostParam( { R_QUALITY } );
	unlockParam  = AddHostParam( { R_UNLOCK } );

	UpdateVisibility( false );
}

ImPlugin::~ImPlugin()
{
}

int ImPlugin::AddHostParam( HostParam p )
{
	unsigned int i = (unsigned int)params.size();
	switch( p.role )
	{
	case R_PROGRESS:
		// Arena drives the parameter called "Opacity" with the transition / crossfader position.
		SetParamInfo( i, "Opacity", FF_TYPE_STANDARD, 0.0f );
		p.value = 0.0f;
		break;
	case R_STYLE:
		SetOptionParamInfo( i, "Style", (unsigned int)family.numStyles, 0.0f );
		for( int s = 0; s < family.numStyles; ++s )
			SetParamElementInfo( i, (unsigned int)s, family.styles[ s ].label, (float)s );
		p.value = 0.0f;
		break;
	case R_STYLE_PARAM:
	{
		const ImStyleDef& st = family.styles[ p.style ];
		std::string name     = family.numStyles > 1 ? std::string( st.label ) + " - " + p.def->label : std::string( p.def->label );
		switch( p.def->kind )
		{
		case IM_SLIDER:
			SetParamInfo( i, name.c_str(), FF_TYPE_STANDARD, p.def->defaultValue );
			break;
		case IM_TOGGLE:
			SetParamInfo( i, name.c_str(), FF_TYPE_BOOLEAN, p.def->defaultValue > 0.5f );
			break;
		case IM_OPTION:
			SetOptionParamInfo( i, name.c_str(), (unsigned int)p.def->numOptions, p.def->defaultValue );
			for( int k = 0; k < p.def->numOptions; ++k )
				SetParamElementInfo( i, (unsigned int)k, p.def->optionLabels[ k ], (float)k );
			break;
		}
		p.value = p.def->defaultValue;
		if( family.numStyles > 1 )
			SetParamDisplayName( i, p.def->label, false );
		SetParamGroup( i, "Look" );
		break;
	}
	case R_WORD:
		p.text = kDefaultWord;
		SetParamInfo( i, "Word", FF_TYPE_TEXT, kDefaultWord );
		SetParamGroup( i, "Text" );
		break;
	case R_FONT:
	{
		int n           = 0;
		const auto* fts = imtext::FontChoices( &n );
		SetOptionParamInfo( i, "Font", (unsigned int)n, 0.0f );
		for( int k = 0; k < n; ++k )
			SetParamElementInfo( i, (unsigned int)k, fts[ k ].label, (float)k );
		SetParamGroup( i, "Text" );
		p.value = 0.0f;
		break;
	}
	case R_BEAT:
		SetParamInfo( i, "Beat punch", FF_TYPE_STANDARD, 1.0f );
		SetParamGroup( i, "Output" );
		p.value = 1.0f;
		break;
	case R_MIX:
		SetParamInfo( i, "Mix", FF_TYPE_STANDARD, 1.0f );
		SetParamGroup( i, "Output" );
		p.value = 1.0f;
		break;
	case R_QUALITY:
		SetOptionParamInfo( i, "Quality", 3, 0.0f );
		for( int k = 0; k < 3; ++k )
			SetParamElementInfo( i, (unsigned int)k, kQualityLabels[ k ], (float)k );
		SetParamGroup( i, "Output" );
		p.value = 0.0f;
		break;
	case R_UNLOCK:
		SetParamInfo( i, "Unlock code", FF_TYPE_TEXT, "" );
		SetParamGroup( i, "Licence" );
		break;
	}
	params.push_back( p );
	return (int)i;
}

int ImPlugin::StyleIndex() const
{
	if( styleParam < 0 )
		return 0;
	int s = (int)std::lround( params[ styleParam ].value );
	return std::max( 0, std::min( family.numStyles - 1, s ) );
}

void ImPlugin::UpdateVisibility( bool raise )
{
	int style      = StyleIndex();
	bool styleText = ( family.styles[ style ].flags & IM_TEXT ) != 0;
	for( size_t i = 0; i < params.size(); ++i )
	{
		const HostParam& p = params[ i ];
		bool visible       = true;
		if( p.role == R_STYLE_PARAM )
			visible = p.style == style;
		else if( p.role == R_WORD || p.role == R_FONT )
			visible = styleText;
		SetParamVisibility( (unsigned int)i, visible, raise );
	}
}

float ImPlugin::ShaderValue( const HostParam& p ) const
{
	switch( p.def->kind )
	{
	case IM_TOGGLE:
		return p.value > 0.5f ? 1.0f : 0.0f;
	case IM_OPTION:
	{
		int k = std::max( 0, std::min( p.def->numOptions - 1, (int)std::lround( p.value ) ) );
		return p.def->optionValues[ k ];
	}
	default:
		return p.value;
	}
}

float ImPlugin::Quality() const
{
	int q = (int)std::lround( params[ qualityParam ].value );
	return kQualityScale[ std::max( 0, std::min( 2, q ) ) ];
}

FFResult ImPlugin::SetFloatParameter( unsigned int index, float value )
{
	if( index >= params.size() )
		return FF_FAIL;
	HostParam& p = params[ index ];
	switch( p.role )
	{
	case R_WORD:
	case R_UNLOCK:
		return FF_FAIL;
	case R_STYLE:
	{
		int before = StyleIndex();
		p.value    = std::round( value );
		if( StyleIndex() != before )
			UpdateVisibility( true );
		break;
	}
	case R_FONT:
	case R_QUALITY:
		p.value = std::round( value );
		break;
	case R_STYLE_PARAM:
		p.value = p.def->kind == IM_OPTION ? std::round( value ) : value;
		break;
	default:
		p.value = value;
		break;
	}
	return FF_SUCCESS;
}

float ImPlugin::GetFloatParameter( unsigned int index )
{
	if( index >= params.size() )
		return 0.0f;
	return params[ index ].value;
}

FFResult ImPlugin::SetTextParameter( unsigned int index, const char* value )
{
	if( index >= params.size() )
		return FF_FAIL;
	HostParam& p = params[ index ];
	if( p.role != R_WORD && p.role != R_UNLOCK )
		return FF_FAIL;
	p.text = value ? value : "";
	if( p.role == R_UNLOCK && !p.text.empty() )
		imlic::TryUnlock( p.text );
	return FF_SUCCESS;
}

char* ImPlugin::GetTextParameter( unsigned int index )
{
	if( index >= params.size() )
		return (char*)FF_FAIL;
	HostParam& p = params[ index ];
	if( p.role != R_WORD && p.role != R_UNLOCK )
		return (char*)FF_FAIL;
	return const_cast< char* >( p.text.c_str() );
}

char* ImPlugin::GetParameterDisplay( unsigned int index )
{
	static char buf[ 64 ];
	if( index >= params.size() )
		return (char*)FF_FAIL;
	const HostParam& p = params[ index ];
	switch( p.role )
	{
	case R_STYLE:
		std::snprintf( buf, sizeof( buf ), "%s", family.styles[ StyleIndex() ].label );
		break;
	case R_FONT:
	{
		int n           = 0;
		const auto* fts = imtext::FontChoices( &n );
		int k           = std::max( 0, std::min( n - 1, (int)std::lround( p.value ) ) );
		std::snprintf( buf, sizeof( buf ), "%s", fts[ k ].label );
		break;
	}
	case R_QUALITY:
		std::snprintf( buf, sizeof( buf ), "%s", kQualityLabels[ std::max( 0, std::min( 2, (int)std::lround( p.value ) ) ) ] );
		break;
	case R_WORD:
	case R_UNLOCK:
		return (char*)FF_FAIL;
	case R_STYLE_PARAM:
		if( p.def->kind == IM_OPTION )
		{
			int k = std::max( 0, std::min( p.def->numOptions - 1, (int)std::lround( p.value ) ) );
			std::snprintf( buf, sizeof( buf ), "%s", p.def->optionLabels[ k ] );
			break;
		}
		if( p.def->kind == IM_TOGGLE )
		{
			std::snprintf( buf, sizeof( buf ), "%s", p.value > 0.5f ? "On" : "Off" );
			break;
		}
		// fall through
	default:
		std::snprintf( buf, sizeof( buf ), "%d%%", (int)std::lround( p.value * 100.0f ) );
		break;
	}
	return buf;
}

// ------------------------------------------------------------------------------------------------ GL helpers

GLuint ImPlugin::Compile( const std::string& fragment, const char* what )
{
	auto compileStage = [ & ]( GLenum type, const char* text ) -> GLuint {
		GLuint s = glCreateShader( type );
		glShaderSource( s, 1, &text, nullptr );
		glCompileShader( s );
		GLint ok = 0;
		glGetShaderiv( s, GL_COMPILE_STATUS, &ok );
		if( !ok )
		{
			char log[ 2048 ] = { 0 };
			glGetShaderInfoLog( s, sizeof( log ) - 1, nullptr, log );
			FFGLLog::LogToHost( ( std::string( "IMMINENCE: shader failed (" ) + what + "): " + log ).c_str() );
			glDeleteShader( s );
			return 0;
		}
		return s;
	};
	GLuint vs = compileStage( GL_VERTEX_SHADER, kImVertex );
	GLuint fs = compileStage( GL_FRAGMENT_SHADER, fragment.c_str() );
	if( !vs || !fs )
	{
		if( vs )
			glDeleteShader( vs );
		if( fs )
			glDeleteShader( fs );
		return 0;
	}
	GLuint prog = glCreateProgram();
	glAttachShader( prog, vs );
	glAttachShader( prog, fs );
	glLinkProgram( prog );
	glDeleteShader( vs );
	glDeleteShader( fs );
	GLint ok = 0;
	glGetProgramiv( prog, GL_LINK_STATUS, &ok );
	if( !ok )
	{
		char log[ 2048 ] = { 0 };
		glGetProgramInfoLog( prog, sizeof( log ) - 1, nullptr, log );
		FFGLLog::LogToHost( ( std::string( "IMMINENCE: link failed (" ) + what + "): " + log ).c_str() );
		glDeleteProgram( prog );
		return 0;
	}
	return prog;
}

bool ImPlugin::EnsureTarget( Target& t, int w, int h, GLenum internalFormat, bool mips, GLenum wrap )
{
	if( t.tex && t.w == w && t.h == h && t.format == internalFormat )
		return false;
	FreeTarget( t );
	t.w      = w;
	t.h      = h;
	t.format = internalFormat;
	t.mips   = mips;
	glGenTextures( 1, &t.tex );
	glBindTexture( GL_TEXTURE_2D, t.tex );
	GLenum type = internalFormat == GL_RGBA16F ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE;
	glTexImage2D( GL_TEXTURE_2D, 0, internalFormat, w, h, 0, GL_RGBA, type, nullptr );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap );
	if( mips )
		glGenerateMipmap( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, 0 );
	glGenFramebuffers( 1, &t.fbo );
	glBindFramebuffer( GL_FRAMEBUFFER, t.fbo );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0 );
	ClearTarget( t );
	return true;
}

void ImPlugin::FreeTarget( Target& t )
{
	if( t.fbo )
		glDeleteFramebuffers( 1, &t.fbo );
	if( t.tex )
		glDeleteTextures( 1, &t.tex );
	t = Target();
}

void ImPlugin::ClearTarget( Target& t )
{
	if( !t.fbo )
		return;
	glBindFramebuffer( GL_FRAMEBUFFER, t.fbo );
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_COLOR_BUFFER_BIT );
}

void ImPlugin::Draw( GLuint program )
{
	glUseProgram( program );
	quad.Draw();
}

void ImPlugin::CopyInput( const FFGLTextureStruct* in, Target& dst )
{
	glBindFramebuffer( GL_FRAMEBUFFER, dst.fbo );
	glViewport( 0, 0, dst.w, dst.h );
	glUseProgram( copyProg );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, in->Handle );
	FFGLTexCoords m = GetMaxGLTexCoords( *in );
	glUniform1i( glGetUniformLocation( copyProg, "uIn" ), 0 );
	glUniform2f( glGetUniformLocation( copyProg, "uMaxUV" ), m.s, m.t );
	quad.Draw();
	glBindTexture( GL_TEXTURE_2D, 0 );
}

ImPlugin::Program* ImPlugin::StyleProgram( int style )
{
	Program& p = stylePrograms[ style ];
	if( p.id || p.failed )
		return &p;
	std::string fs;
	for( const char* const* c = kImPrelude; *c; ++c )
		fs += *c;
	for( const char* const* c = family.styles[ style ].source; *c; ++c )
		fs += *c;
	fs += kImMain;
	std::string what = std::string( family.name ) + " / " + family.styles[ style ].label;
	p.id             = Compile( fs, what.c_str() );
	p.failed         = p.id == 0;
	return &p;
}

void ImPlugin::UpdateTextTextures( unsigned flags )
{
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	if( flags & IM_TEXT )
	{
		std::string word = wordParam >= 0 ? params[ wordParam ].text : kDefaultWord;
		int font         = fontParam >= 0 ? (int)std::lround( params[ fontParam ].value ) : 0;
		if( !textTex || word != renderedWord || font != renderedFont )
		{
			std::vector< unsigned char > img;
			imtext::RenderWord( word, font, img );
			if( !textTex )
				glGenTextures( 1, &textTex );
			glBindTexture( GL_TEXTURE_2D, textTex );
			glTexImage2D( GL_TEXTURE_2D, 0, GL_R8, imtext::kWordW, imtext::kWordH, 0, GL_RED, GL_UNSIGNED_BYTE, img.data() );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
			glGenerateMipmap( GL_TEXTURE_2D );
			glBindTexture( GL_TEXTURE_2D, 0 );
			renderedWord = word;
			renderedFont = font;
		}
	}
	if( ( flags & IM_GLYPH ) && !glyphsReady )
	{
		std::vector< unsigned char > img;
		imtext::RenderGlyphAtlas( img );
		if( !glyphTex )
			glGenTextures( 1, &glyphTex );
		glBindTexture( GL_TEXTURE_2D, glyphTex );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_R8, imtext::kGlyphW, imtext::kGlyphH, 0, GL_RED, GL_UNSIGNED_BYTE, img.data() );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glBindTexture( GL_TEXTURE_2D, 0 );
		glyphsReady = true;
	}
	glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
}

void ImPlugin::PushHistory()
{
	int w = std::min( src.w, 768 );
	int h = std::max( 2, (int)std::lround( (double)w * src.h / std::max( 1, src.w ) ) );
	if( !histTex || w != histW || h != histH )
	{
		if( histTex )
			glDeleteTextures( 1, &histTex );
		glGenTextures( 1, &histTex );
		glBindTexture( GL_TEXTURE_2D_ARRAY, histTex );
		glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, w, h, kHistFrames, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
		glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
		if( !histFbo )
			glGenFramebuffers( 1, &histFbo );
		glBindFramebuffer( GL_FRAMEBUFFER, histFbo );
		for( int l = 0; l < kHistFrames; ++l )
		{
			glFramebufferTextureLayer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, histTex, 0, l );
			glClearColor( 0, 0, 0, 0 );
			glClear( GL_COLOR_BUFFER_BIT );
		}
		histW    = w;
		histH    = h;
		histHead = 0;
	}
	histHead = ( histHead + 1 ) % kHistFrames;
	glBindFramebuffer( GL_READ_FRAMEBUFFER, src.fbo );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, histFbo );
	glFramebufferTextureLayer( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, histTex, 0, histHead );
	glBlitFramebuffer( 0, 0, src.w, src.h, 0, 0, histW, histH, GL_COLOR_BUFFER_BIT, GL_LINEAR );
}

void ImPlugin::UpdateClock()
{
	auto now  = std::chrono::steady_clock::now();
	double dt = std::chrono::duration< double >( now - lastFrame ).count();
	lastFrame = now;
	dt        = std::max( 0.0, std::min( 0.25, dt ) );
	seconds += dt;
	if( dt > 1e-4 )
		fps += ( (float)std::min( 240.0, 1.0 / dt ) - fps ) * 0.05f;

	// Follow Arena's beat: count bars from the bar phase it sends. If it stops sending, keep time from the BPM.
	float tempo = bpm > 1.0f ? bpm : 120.0f;
	if( barPhase + 0.5f < lastBarPhase )
		barCount += 1.0;
	bool phaseMoved = std::fabs( barPhase - lastBarPhase ) > 1e-6f;
	lastBarPhase    = barPhase;
	if( phaseMoved )
		beats = barCount * 4.0 + barPhase * 4.0;
	else
		beats += dt * tempo / 60.0;
}

void ImPlugin::RestoreState( GLuint hostFbo )
{
	for( int u = 0; u < kMaxUnits; ++u )
	{
		glActiveTexture( GL_TEXTURE0 + u );
		glBindTexture( GL_TEXTURE_2D, 0 );
		glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
	}
	glActiveTexture( GL_TEXTURE0 );
	glUseProgram( 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, hostFbo );
}

// ------------------------------------------------------------------------------------------------ lifecycle

FFResult ImPlugin::InitGL( const FFGLViewportStruct* vp )
{
	if( !quad.Initialise() )
		return FF_FAIL;
	copyProg  = Compile( kImCopy, "copy" );
	compProg  = Compile( kImComposite, "composite" );
	clearProg = Compile( kImClear, "clear" );
	if( !copyProg || !compProg || !clearProg )
	{
		DeInitGL();
		return FF_FAIL;
	}
	// 1x1 stand-ins so every sampler always has a texture of the right type bound
	const unsigned char black[ 4 ] = { 0, 0, 0, 0 };
	glGenTextures( 1, &dummy2D );
	glBindTexture( GL_TEXTURE_2D, dummy2D );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glBindTexture( GL_TEXTURE_2D, 0 );
	glGenTextures( 1, &dummyArray );
	glBindTexture( GL_TEXTURE_2D_ARRAY, dummyArray );
	glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
	return CFFGLPlugin::InitGL( vp );
}

FFResult ImPlugin::DeInitGL()
{
	for( auto& kv : stylePrograms )
		if( kv.second.id )
			glDeleteProgram( kv.second.id );
	stylePrograms.clear();
	for( GLuint* p : { &copyProg, &compProg, &clearProg } )
		if( *p )
		{
			glDeleteProgram( *p );
			*p = 0;
		}
	FreeTarget( src );
	FreeTarget( src2 );
	FreeTarget( out[ 0 ] );
	FreeTarget( out[ 1 ] );
	for( GLuint* t : { &histTex, &textTex, &glyphTex, &dummy2D, &dummyArray } )
		if( *t )
		{
			glDeleteTextures( 1, t );
			*t = 0;
		}
	if( histFbo )
	{
		glDeleteFramebuffers( 1, &histFbo );
		histFbo = 0;
	}
	quad.Release();
	return FF_SUCCESS;
}

// ------------------------------------------------------------------------------------------------ render

FFResult ImPlugin::ProcessOpenGL( ProcessOpenGLStruct* pGL )
{
	const FFGLTextureStruct* inA = nullptr;
	const FFGLTextureStruct* inB = nullptr;
	if( family.kind == IM_KIND_EFFECT )
	{
		if( pGL->numInputTextures < 1 || !pGL->inputTextures[ 0 ] )
			return FF_FAIL;
		inA = pGL->inputTextures[ 0 ];
	}
	else if( family.kind == IM_KIND_MIXER )
	{
		if( pGL->numInputTextures < 2 || !pGL->inputTextures[ 0 ] || !pGL->inputTextures[ 1 ] )
			return FF_FAIL;
		inA = pGL->inputTextures[ 0 ];
		inB = pGL->inputTextures[ 1 ];
	}

	UpdateClock();
	bool locked = !imlic::IsUnlocked();
	if( !locked && !unlockedShown )
	{
		SetParamDisplayName( (unsigned int)unlockParam, "Unlocked", true );
		unlockedShown = true;
	}

	int W = std::max( 1, (int)currentViewport.width );
	int H = std::max( 1, (int)currentViewport.height );
	float q = Quality();
	int IW  = std::max( 8, (int)std::lround( W * q ) );
	int IH  = std::max( 8, (int)std::lround( H * q ) );

	int style            = StyleIndex();
	const ImStyleDef& sd = family.styles[ style ];
	unsigned flags       = sd.flags;

	GLboolean wasBlend   = glIsEnabled( GL_BLEND );
	GLboolean wasScissor = glIsEnabled( GL_SCISSOR_TEST );
	GLboolean wasDepth   = glIsEnabled( GL_DEPTH_TEST );
	glDisable( GL_BLEND );
	glDisable( GL_SCISSOR_TEST );
	glDisable( GL_DEPTH_TEST );

	bool resized = EnsureTarget( src, IW, IH, GL_RGBA8, true, GL_MIRRORED_REPEAT );
	if( family.kind == IM_KIND_MIXER || ( flags & IM_SECOND ) )
		resized |= EnsureTarget( src2, IW, IH, GL_RGBA8, true, GL_MIRRORED_REPEAT );
	resized |= EnsureTarget( out[ 0 ], IW, IH, GL_RGBA16F, false, GL_CLAMP_TO_EDGE );
	if( flags & IM_FEEDBACK )
		resized |= EnsureTarget( out[ 1 ], IW, IH, GL_RGBA16F, false, GL_CLAMP_TO_EDGE );
	if( resized || style != lastStyle )
	{
		ClearTarget( out[ 0 ] );
		ClearTarget( out[ 1 ] );
		srcCleared = false;
	}
	lastStyle = style;

	// 1. inputs, copied into our own textures so the styles can blur them with mipmaps
	if( family.kind == IM_KIND_SOURCE )
	{
		if( !srcCleared )
			ClearTarget( src );
		srcCleared = true;
	}
	else
		CopyInput( inA, src );
	if( inB )
		CopyInput( inB, src2 );
	glBindTexture( GL_TEXTURE_2D, src.tex );
	glGenerateMipmap( GL_TEXTURE_2D );
	if( inB )
	{
		glBindTexture( GL_TEXTURE_2D, src2.tex );
		glGenerateMipmap( GL_TEXTURE_2D );
	}
	glBindTexture( GL_TEXTURE_2D, 0 );
	if( flags & IM_HISTORY )
		PushHistory();
	if( flags & ( IM_TEXT | IM_GLYPH ) )
		UpdateTextTextures( flags );

	// 2. the style itself
	int write    = ( flags & IM_FEEDBACK ) ? cur : 0;
	int read     = 1 - cur;
	Program* prg = StyleProgram( style );
	glBindFramebuffer( GL_FRAMEBUFFER, out[ write ].fbo );
	glViewport( 0, 0, IW, IH );
	if( prg && prg->id )
	{
		glUseProgram( prg->id );
		struct Bind
		{
			const char* name;
			GLenum target;
			GLuint tex;
		} binds[] = {
			{ "uSrc", GL_TEXTURE_2D, src.tex },
			{ "uPrev", GL_TEXTURE_2D, ( flags & IM_FEEDBACK ) ? out[ read ].tex : dummy2D },
			{ "uText", GL_TEXTURE_2D, textTex ? textTex : dummy2D },
			{ "uClip", GL_TEXTURE_2D, dummy2D },
			{ "uHist", GL_TEXTURE_2D_ARRAY, histTex ? histTex : dummyArray },
			{ "uGlyph", GL_TEXTURE_2D, glyphTex ? glyphTex : dummy2D },
			{ "uSrc2", GL_TEXTURE_2D, src2.tex ? src2.tex : dummy2D },
		};
		for( int u = 0; u < (int)( sizeof( binds ) / sizeof( binds[ 0 ] ) ); ++u )
		{
			glActiveTexture( GL_TEXTURE0 + u );
			glBindTexture( binds[ u ].target, binds[ u ].tex );
			glUniform1i( prg->Loc( binds[ u ].name ), u );
		}
		glActiveTexture( GL_TEXTURE0 );

		float pv[ 8 ] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		for( const HostParam& p : params )
			if( p.role == R_STYLE_PARAM && p.style == style && p.def->slot >= 0 && p.def->slot < 8 )
				pv[ p.def->slot ] = ShaderValue( p );
		static const char* slotNames[ 8 ] = { "uP0", "uP1", "uP2", "uP3", "uP4", "uP5", "uP6", "uP7" };
		for( int k = 0; k < 8; ++k )
			glUniform1f( prg->Loc( slotNames[ k ] ), pv[ k ] );

		float tempo = bpm > 1.0f ? bpm : 120.0f;
		glUniform2f( prg->Loc( "uRes" ), (float)IW, (float)IH );
		glUniform1f( prg->Loc( "uTime" ), (float)std::fmod( seconds, 3600.0 ) );
		glUniform1f( prg->Loc( "uBeats" ), (float)std::fmod( beats, 4096.0 ) );
		glUniform1f( prg->Loc( "uBeat" ), (float)( beats - std::floor( beats ) ) );
		glUniform1f( prg->Loc( "uClipAspect" ), 16.0f / 9.0f );
		glUniform1f( prg->Loc( "uBeatAmt" ), beatParam >= 0 ? params[ beatParam ].value : 1.0f );
		glUniform1f( prg->Loc( "uProgress" ), progressParam >= 0 ? params[ progressParam ].value : 0.0f );
		glUniform1f( prg->Loc( "uHead" ), (float)histHead );
		glUniform1f( prg->Loc( "uFPB" ), fps * 60.0f / tempo );
		quad.Draw();
	}
	else
	{
		// style failed to compile: pass the input through untouched rather than going black
		glUseProgram( copyProg );
		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_2D, inB && progressParam >= 0 && params[ progressParam ].value > 0.5f ? src2.tex : src.tex );
		glUniform1i( glGetUniformLocation( copyProg, "uIn" ), 0 );
		glUniform2f( glGetUniformLocation( copyProg, "uMaxUV" ), 1.0f, 1.0f );
		quad.Draw();
	}
	if( flags & IM_FEEDBACK )
		cur = 1 - cur;

	// 3. composite into Arena's framebuffer: mix with the original, alpha, watermark
	glBindFramebuffer( GL_FRAMEBUFFER, pGL->HostFBO );
	glViewport( currentViewport.x, currentViewport.y, W, H );
	glUseProgram( compProg );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, out[ write ].tex );
	glActiveTexture( GL_TEXTURE1 );
	glBindTexture( GL_TEXTURE_2D, inA ? inA->Handle : dummy2D );
	glActiveTexture( GL_TEXTURE2 );
	glBindTexture( GL_TEXTURE_2D, inB ? inB->Handle : dummy2D );
	glActiveTexture( GL_TEXTURE0 );
	FFGLTexCoords ma = inA ? GetMaxGLTexCoords( *inA ) : FFGLTexCoords{ 1.0f, 1.0f };
	FFGLTexCoords mb = inB ? GetMaxGLTexCoords( *inB ) : FFGLTexCoords{ 1.0f, 1.0f };
	glUniform1i( glGetUniformLocation( compProg, "uFx" ), 0 );
	glUniform1i( glGetUniformLocation( compProg, "uOrig" ), 1 );
	glUniform1i( glGetUniformLocation( compProg, "uOrigB" ), 2 );
	glUniform2f( glGetUniformLocation( compProg, "uOrigMaxUV" ), ma.s, ma.t );
	glUniform2f( glGetUniformLocation( compProg, "uOrigBMaxUV" ), mb.s, mb.t );
	glUniform1f( glGetUniformLocation( compProg, "uKind" ), (float)family.kind );
	glUniform1f( glGetUniformLocation( compProg, "uMix" ), mixParam >= 0 ? params[ mixParam ].value : 1.0f );
	glUniform1f( glGetUniformLocation( compProg, "uProgress" ), progressParam >= 0 ? params[ progressParam ].value : 0.0f );
	glUniform1f( glGetUniformLocation( compProg, "uLocked" ), locked ? 1.0f : 0.0f );
	glUniform1f( glGetUniformLocation( compProg, "uTime" ), (float)std::fmod( seconds, 3600.0 ) );
	glUniform2f( glGetUniformLocation( compProg, "uRes" ), (float)W, (float)H );
	quad.Draw();

	RestoreState( pGL->HostFBO );
	if( wasBlend )
		glEnable( GL_BLEND );
	if( wasScissor )
		glEnable( GL_SCISSOR_TEST );
	if( wasDepth )
		glEnable( GL_DEPTH_TEST );
	return FF_SUCCESS;
}
