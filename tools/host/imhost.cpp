// IMMINENCE test host (Linux only). Loads a built plugin exactly like an FFGL host would (through plugMain),
// feeds it the preview's Stage cam / Neon loop, drives controls, beat and transition progress, and saves PNGs.
// It also checks that the plugin leaves the OpenGL state the way FFGL requires.
//
//   imhost <plugin.so> [--list] [--style N] [--set index=value] [--text index=string] [--unlock code]
//          [--source stage|loop] [--size WxH] [--frames N] [--shots a,b,c] [--progress p | --sweep]
//          [--out file_prefix] [--bpm N]
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include <ffgl/FFGL.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "sources_gen.h"

typedef FFMixed ( *PlugMain )( FFUInt32, FFMixed, FFInstanceID );
static PlugMain plug;

static FFMixed Call( FFUInt32 code, FFMixed in, FFInstanceID inst )
{
	return plug( code, in, inst );
}
static FFMixed U( FFUInt32 v )
{
	FFMixed m;
	m.PointerValue = nullptr;
	m.UIntValue    = v;
	return m;
}
static FFMixed P( void* p )
{
	FFMixed m;
	m.PointerValue = p;
	return m;
}

static GLuint CompileProgram( const std::string& fs )
{
	const char* vs = "#version 410 core\nlayout(location=0) in vec2 p; out vec2 vUv; void main(){ vUv=p*.5+.5; gl_Position=vec4(p,0.,1.); }";
	auto stage     = [ & ]( GLenum t, const char* src ) {
        GLuint s = glCreateShader( t );
        glShaderSource( s, 1, &src, nullptr );
        glCompileShader( s );
        GLint ok;
        glGetShaderiv( s, GL_COMPILE_STATUS, &ok );
        if( !ok )
        {
            char log[ 4096 ];
            glGetShaderInfoLog( s, sizeof( log ), nullptr, log );
            fprintf( stderr, "host shader error: %s\n", log );
            exit( 2 );
        }
        return s;
	};
	GLuint p = glCreateProgram();
	glAttachShader( p, stage( GL_VERTEX_SHADER, vs ) );
	glAttachShader( p, stage( GL_FRAGMENT_SHADER, fs.c_str() ) );
	glLinkProgram( p );
	return p;
}

struct Fbo
{
	GLuint fbo = 0, tex = 0;
	void Make( int w, int h )
	{
		glGenTextures( 1, &tex );
		glBindTexture( GL_TEXTURE_2D, tex );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glBindTexture( GL_TEXTURE_2D, 0 );
		glGenFramebuffers( 1, &fbo );
		glBindFramebuffer( GL_FRAMEBUFFER, fbo );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}
};

static int ParamIndex( FFInstanceID inst, int n, const char* name )
{
	for( int i = 0; i < n; ++i )
	{
		const char* pn = (const char*)Call( FF_GET_PARAMETER_NAME, U( i ), inst ).PointerValue;
		if( pn && strcmp( pn, name ) == 0 )
			return i;
	}
	return -1;
}

static void SetFloat( FFInstanceID inst, int i, float v )
{
	SetParameterStruct sp;
	memset( &sp, 0, sizeof( sp ) );
	sp.ParameterNumber = i;
	memcpy( &sp.NewParameterValue.UIntValue, &v, 4 );
	Call( FF_SET_PARAMETER, P( &sp ), inst );
}
static void SetText( FFInstanceID inst, int i, const char* t )
{
	SetParameterStruct sp;
	memset( &sp, 0, sizeof( sp ) );
	sp.ParameterNumber              = i;
	sp.NewParameterValue.PointerValue = (void*)t;
	Call( FF_SET_PARAMETER, P( &sp ), inst );
}

static bool CheckState( GLuint hostFbo, const char* when )
{
	bool ok = true;
	GLint v;
	glGetIntegerv( GL_CURRENT_PROGRAM, &v );
	if( v != 0 ) { fprintf( stderr, "STATE %s: program %d left bound\n", when, v ); ok = false; }
	glGetIntegerv( GL_ACTIVE_TEXTURE, &v );
	if( v != GL_TEXTURE0 ) { fprintf( stderr, "STATE %s: active texture not 0\n", when ); ok = false; }
	glGetIntegerv( GL_FRAMEBUFFER_BINDING, &v );
	if( (GLuint)v != hostFbo ) { fprintf( stderr, "STATE %s: framebuffer %d bound, host is %u\n", when, v, hostFbo ); ok = false; }
	glGetIntegerv( GL_ARRAY_BUFFER_BINDING, &v );
	if( v != 0 ) { fprintf( stderr, "STATE %s: array buffer left bound\n", when ); ok = false; }
	glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &v );
	if( v != 0 ) { fprintf( stderr, "STATE %s: VAO left bound\n", when ); ok = false; }
	for( int u = 0; u < 16; ++u )
	{
		glActiveTexture( GL_TEXTURE0 + u );
		GLint t2, ta;
		glGetIntegerv( GL_TEXTURE_BINDING_2D, &t2 );
		glGetIntegerv( GL_TEXTURE_BINDING_2D_ARRAY, &ta );
		if( t2 || ta ) { fprintf( stderr, "STATE %s: texture left bound on unit %d\n", when, u ); ok = false; }
	}
	glActiveTexture( GL_TEXTURE0 );
	GLenum e = glGetError();
	if( e != GL_NO_ERROR ) { fprintf( stderr, "GL ERROR %s: 0x%x\n", when, e ); ok = false; }
	return ok;
}

int main( int argc, char** argv )
{
	if( argc < 2 )
	{
		fprintf( stderr, "usage: imhost plugin.so [options]\n" );
		return 1;
	}
	std::string path = argv[ 1 ], source = "stage", out = "shot", unlock;
	int W = 960, H = 540, frames = 40, style = -1;
	float bpm = 124.f, progress = -1.f;
	bool list = false, sweep = false;
	std::vector< std::pair< int, float > > sets;
	std::vector< std::pair< int, std::string > > texts;
	std::set< int > shots;
	for( int i = 2; i < argc; ++i )
	{
		std::string a = argv[ i ];
		auto next     = [ & ]() { return std::string( i + 1 < argc ? argv[ ++i ] : "" ); };
		if( a == "--list" ) list = true;
		else if( a == "--style" ) style = atoi( next().c_str() );
		else if( a == "--set" ) { std::string v = next(); auto k = v.find( '=' ); sets.push_back( { atoi( v.substr( 0, k ).c_str() ), (float)atof( v.substr( k + 1 ).c_str() ) } ); }
		else if( a == "--text" ) { std::string v = next(); auto k = v.find( '=' ); texts.push_back( { atoi( v.substr( 0, k ).c_str() ), v.substr( k + 1 ) } ); }
		else if( a == "--unlock" ) unlock = next();
		else if( a == "--source" ) source = next();
		else if( a == "--size" ) { std::string v = next(); sscanf( v.c_str(), "%dx%d", &W, &H ); }
		else if( a == "--frames" ) frames = atoi( next().c_str() );
		else if( a == "--shots" ) { std::string v = next(); size_t s = 0; while( s < v.size() ) { size_t e = v.find( ',', s ); if( e == std::string::npos ) e = v.size(); shots.insert( atoi( v.substr( s, e - s ).c_str() ) ); s = e + 1; } }
		else if( a == "--progress" ) progress = (float)atof( next().c_str() );
		else if( a == "--sweep" ) sweep = true;
		else if( a == "--out" ) out = next();
		else if( a == "--bpm" ) bpm = (float)atof( next().c_str() );
	}
	if( shots.empty() )
		shots.insert( frames - 1 );

	if( !glfwInit() ) { fprintf( stderr, "glfw init failed (is DISPLAY set?)\n" ); return 1; }
	glfwWindowHint( GLFW_VISIBLE, GLFW_FALSE );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 1 );
	glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
	glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE );
	GLFWwindow* win = glfwCreateWindow( 64, 64, "imhost", nullptr, nullptr );
	if( !win ) { fprintf( stderr, "no GL 4.1 context\n" ); return 1; }
	glfwMakeContextCurrent( win );
	glewExperimental = GL_TRUE;
	glewInit();
	glGetError();

	void* lib = dlopen( path.c_str(), RTLD_NOW | RTLD_LOCAL );
	if( !lib ) { fprintf( stderr, "dlopen: %s\n", dlerror() ); return 1; }
	plug = (PlugMain)dlsym( lib, "plugMain" );
	if( !plug ) { fprintf( stderr, "no plugMain\n" ); return 1; }

	if( Call( FF_INITIALISE_V2, U( 0 ), nullptr ).UIntValue != FF_SUCCESS ) { fprintf( stderr, "initialise failed\n" ); return 1; }
	PluginInfoStruct* info = (PluginInfoStruct*)Call( FF_GET_INFO, U( 0 ), nullptr ).PointerValue;
	char name[ 17 ] = { 0 };
	memcpy( name, info->PluginName, 16 );
	int n = (int)Call( FF_GET_NUM_PARAMETERS, U( 0 ), nullptr ).UIntValue;

	FFGLViewportStruct vp = { 0, 0, (GLuint)W, (GLuint)H };
	FFInstanceID inst     = Call( FF_INSTANTIATE_GL, P( &vp ), nullptr ).PointerValue;
	if( !inst || inst == (void*)(uintptr_t)FF_FAIL ) { fprintf( stderr, "instantiate failed\n" ); return 1; }

	int styleIdx = ParamIndex( inst, n, "Style" );
	if( style >= 0 && styleIdx >= 0 )
		SetFloat( inst, styleIdx, (float)style );
	for( auto& s : sets ) SetFloat( inst, s.first, s.second );
	for( auto& t : texts ) SetText( inst, t.first, t.second.c_str() );
	if( !unlock.empty() ) SetText( inst, ParamIndex( inst, n, "Unlock code" ), unlock.c_str() );

	if( list )
	{
		printf( "%s  (%s)  type=%u  params=%d\n", name, info->PluginUniqueID[ 0 ] ? std::string( info->PluginUniqueID, 4 ).c_str() : "", info->PluginType, n );
		for( int i = 0; i < n; ++i )
		{
			const char* pn = (const char*)Call( FF_GET_PARAMETER_NAME, U( i ), inst ).PointerValue;
			unsigned type  = Call( FF_GET_PARAMETER_TYPE, U( i ), nullptr ).UIntValue;
			unsigned vis   = Call( FF_GET_PRAMETER_VISIBILITY, U( i ), inst ).UIntValue;
			char dn[ 256 ] = { 0 }, grp[ 256 ] = { 0 };
			GetStringStruct gs;
			gs.parameterNumber          = i;
			gs.stringBuffer.address     = dn;
			gs.stringBuffer.maxToWrite  = 255;
			Call( FF_GET_PARAM_DISPLAY_NAME, P( &gs ), inst );
			gs.stringBuffer.address = grp;
			Call( FF_GET_PARAM_GROUP, P( &gs ), inst );
			const char* disp = (const char*)Call( FF_GET_PARAMETER_DISPLAY, U( i ), inst ).PointerValue;
			unsigned ne      = Call( FF_GET_NUM_PARAMETER_ELEMENTS, U( i ), nullptr ).UIntValue;
			printf( "%3d %-34s shown-as=%-24s group=%-8s type=%-3u %s value=%s%s\n", i, pn, dn, grp, type, vis ? "VISIBLE" : "hidden ",
					( disp && disp != (char*)(uintptr_t)FF_FAIL ) ? disp : "-", ne > 1 && ne != FF_FAIL ? ( " (" + std::to_string( ne ) + " choices)" ).c_str() : "" );
		}
	}

	// sources
	std::string pre;
	for( const char* const* c = kHostPrelude; *c; ++c ) pre += *c;
	GLuint progStage = 0, progLoop = 0;
	{
		std::string a = pre, b = pre;
		for( const char* const* c = kHostSrc_stage; *c; ++c ) a += *c;
		for( const char* const* c = kHostSrc_loop; *c; ++c ) b += *c;
		progStage = CompileProgram( a + kHostMain );
		progLoop  = CompileProgram( b + kHostMain );
	}
	GLuint vao, vbo;
	float tri[] = { -1, -1, 3, -1, -1, 3 };
	glGenVertexArrays( 1, &vao );
	glGenBuffers( 1, &vbo );
	glBindVertexArray( vao );
	glBindBuffer( GL_ARRAY_BUFFER, vbo );
	glBufferData( GL_ARRAY_BUFFER, sizeof( tri ), tri, GL_STATIC_DRAW );
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, nullptr );
	glBindVertexArray( 0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );

	Fbo srcA, srcB, host;
	srcA.Make( W, H );
	srcB.Make( W, H );
	host.Make( W, H );
	bool mixer = info->PluginType == FF_MIXER;
	GLuint progA = source == "loop" ? progLoop : progStage;
	GLuint progB = source == "loop" ? progStage : progLoop;
	int opacityIdx = ParamIndex( inst, n, "Opacity" );

	bool stateOk = true;
	std::vector< unsigned char > px( (size_t)W * H * 4 ), row( (size_t)W * 4 );
	for( int f = 0; f < frames; ++f )
	{
		float t     = f / 30.0f;
		float beats = t * bpm / 60.0f;
		auto renderSrc = [ & ]( GLuint prog, Fbo& dst ) {
			glBindFramebuffer( GL_FRAMEBUFFER, dst.fbo );
			glViewport( 0, 0, W, H );
			glUseProgram( prog );
			glUniform2f( glGetUniformLocation( prog, "uRes" ), (float)W, (float)H );
			glUniform1f( glGetUniformLocation( prog, "uTime" ), t );
			glUniform1f( glGetUniformLocation( prog, "uBeats" ), beats );
			glUniform1f( glGetUniformLocation( prog, "uBeat" ), beats - floorf( beats ) );
			glUniform1f( glGetUniformLocation( prog, "uBeatAmt" ), 1.f );
			glBindVertexArray( vao );
			glDrawArrays( GL_TRIANGLES, 0, 3 );
			glBindVertexArray( 0 );
			glUseProgram( 0 );
		};
		renderSrc( progA, srcA );
		if( mixer ) renderSrc( progB, srcB );

		SetBeatinfoStruct bi;
		bi.bpm      = bpm;
		bi.barPhase = fmodf( beats / 4.0f, 1.0f );
		Call( FF_SET_BEATINFO, P( &bi ), inst );
		if( mixer && opacityIdx >= 0 )
			SetFloat( inst, opacityIdx, sweep ? (float)f / std::max( 1, frames - 1 ) : ( progress >= 0 ? progress : 0.5f ) );

		FFGLTextureStruct ta = { (FFUInt32)W, (FFUInt32)H, (FFUInt32)W, (FFUInt32)H, srcA.tex };
		FFGLTextureStruct tb = { (FFUInt32)W, (FFUInt32)H, (FFUInt32)W, (FFUInt32)H, srcB.tex };
		FFGLTextureStruct* arr[ 2 ] = { &ta, &tb };
		ProcessOpenGLStruct pgl;
		pgl.numInputTextures = info->PluginType == FF_SOURCE ? 0 : ( mixer ? 2 : 1 );
		pgl.inputTextures    = arr;
		pgl.HostFBO          = host.fbo;
		glBindFramebuffer( GL_FRAMEBUFFER, host.fbo );
		glViewport( 0, 0, W, H );
		glClearColor( 0, 0, 0, 0 );
		glClear( GL_COLOR_BUFFER_BIT );
		FFUInt32 r = Call( FF_PROCESS_OPENGL, P( &pgl ), inst ).UIntValue;
		if( r != FF_SUCCESS ) { fprintf( stderr, "process failed at frame %d\n", f ); return 3; }
		if( f == 0 || f == frames - 1 )
			stateOk &= CheckState( host.fbo, f == 0 ? "first frame" : "last frame" );

		if( shots.count( f ) )
		{
			glBindFramebuffer( GL_FRAMEBUFFER, host.fbo );
			glReadPixels( 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data() );
			double alphaSum = 0;
			for( int y = 0; y < H / 2; ++y )
			{
				memcpy( row.data(), &px[ (size_t)y * W * 4 ], W * 4 );
				memcpy( &px[ (size_t)y * W * 4 ], &px[ (size_t)( H - 1 - y ) * W * 4 ], W * 4 );
				memcpy( &px[ (size_t)( H - 1 - y ) * W * 4 ], row.data(), W * 4 );
			}
			for( size_t k = 0; k < px.size(); k += 4 ) { alphaSum += px[ k + 3 ]; px[ k + 3 ] = 255; }
			char fn[ 512 ];
			snprintf( fn, sizeof( fn ), "%s_f%03d.png", out.c_str(), f );
			stbi_write_png( fn, W, H, 4, px.data(), W * 4 );
			printf( "wrote %s (mean alpha %.2f)\n", fn, alphaSum / ( (double)W * H * 255.0 ) );
		}
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}
	Call( FF_DEINSTANTIATE_GL, U( 0 ), inst );
	Call( FF_DEINITIALISE, U( 0 ), nullptr );
	printf( "%s: %s\n", name, stateOk ? "GL state OK" : "GL STATE PROBLEMS" );
	return stateOk ? 0 : 4;
}
