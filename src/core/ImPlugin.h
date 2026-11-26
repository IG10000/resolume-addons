#pragma once
#include <ffgl/FFGLPluginSDK.h>
#include <ffgl/FFGLLib.h>
#include <ffgl/FFGLLog.h>
#include <ffglex/FFGLScreenQuad.h>
#include <chrono>
#include <map>
#include <string>
#include <vector>
#include "ImDefs.h"

// One class runs every IMMINENCE plugin. The family table (generated from the preview) decides whether it is an
// effect, a source or a transition, which styles it offers and which controls each style shows.
class ImPlugin : public CFFGLPlugin
{
public:
	explicit ImPlugin( const ImFamilyDef& family );
	~ImPlugin() override;

	FFResult InitGL( const FFGLViewportStruct* vp ) override;
	FFResult DeInitGL() override;
	FFResult ProcessOpenGL( ProcessOpenGLStruct* pGL ) override;

	FFResult SetFloatParameter( unsigned int index, float value ) override;
	float GetFloatParameter( unsigned int index ) override;
	FFResult SetTextParameter( unsigned int index, const char* value ) override;
	char* GetTextParameter( unsigned int index ) override;
	char* GetParameterDisplay( unsigned int index ) override;

private:
	enum Role
	{
		R_PROGRESS,// transitions: Arena's "Opacity", the crossfade position
		R_STYLE,
		R_STYLE_PARAM,
		R_WORD,
		R_FONT,
		R_BEAT,
		R_MIX,
		R_BACKGROUND,// sources: see-through or black behind the drawing
		R_QUALITY,
		R_UNLOCK
	};
	struct HostParam
	{
		Role role;
		int style = -1;
		const ImParamDef* def = nullptr;
		float value = 0.0f;
		std::string text;
	};
	struct Target
	{
		GLuint fbo = 0, tex = 0;
		int w = 0, h = 0;
		GLenum format = 0;
		bool mips = false;
	};
	struct Program
	{
		GLuint id = 0;
		bool failed = false;
		std::map< std::string, GLint > locs;
		GLint Loc( const char* name );
	};

	int AddHostParam( HostParam p );
	int StyleIndex() const;
	void UpdateVisibility( bool raise );
	float ShaderValue( const HostParam& p ) const;
	float Quality() const;

	bool EnsureTarget( Target& t, int w, int h, GLenum internalFormat, bool mips, GLenum wrap );
	void FreeTarget( Target& t );
	void ClearTarget( Target& t );
	Program* StyleProgram( int style );
	GLuint Compile( const std::string& fragment, const char* what );
	void CopyInput( const FFGLTextureStruct* in, Target& dst );
	void UpdateTextTextures( unsigned flags );
	void PushHistory();
	void UpdateClock();
	void Draw( GLuint program );
	void RestoreState( GLuint hostFbo );

	const ImFamilyDef& family;
	std::vector< HostParam > params;
	int styleParam = -1, wordParam = -1, fontParam = -1, beatParam = -1, mixParam = -1, qualityParam = -1;
	int backgroundParam = -1;
	int unlockParam = -1, progressParam = -1;
	bool needsText = false;

	// GL
	ffglex::FFGLScreenQuad quad;
	GLuint copyProg = 0, compProg = 0, clearProg = 0;
	std::map< int, Program > stylePrograms;
	Target src, src2, out[ 2 ];
	GLuint histTex = 0, histFbo = 0;
	int histW = 0, histH = 0, histHead = 0;
	GLuint textTex = 0, glyphTex = 0, dummy2D = 0, dummyArray = 0;
	int cur = 0, lastStyle = -1, lastW = 0, lastH = 0;
	bool srcCleared = false;
	std::string renderedWord;
	int renderedFont = -1;
	bool glyphsReady = false;

	// clock
	std::chrono::steady_clock::time_point startTime, lastFrame;
	double seconds = 0.0;
	double beats = 0.0;
	float lastBarPhase = 0.0f;
	double barCount = 0.0;
	float fps = 60.0f;
	bool unlockedShown = false;
};

// Defines the plugin class and the FFGL registration for one family table.
#define IM_DEFINE_PLUGIN( FAMILY )                                                                  \
	class ImFamilyPlugin : public ImPlugin                                                          \
	{                                                                                               \
	public:                                                                                         \
		ImFamilyPlugin() :                                                                          \
			ImPlugin( FAMILY )                                                                      \
		{                                                                                           \
		}                                                                                           \
	};                                                                                              \
	static CFFGLPluginInfo s_imPluginInfo( PluginFactory< ImFamilyPlugin >, FAMILY.id, FAMILY.name, \
										   2, 1, 1, 0,                                              \
										   FAMILY.kind == IM_KIND_EFFECT   ? FF_EFFECT              \
										   : FAMILY.kind == IM_KIND_SOURCE ? FF_SOURCE              \
																		   : FF_MIXER,              \
										   FAMILY.description, "IMMINENCE VJ Pack" );
