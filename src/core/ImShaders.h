#pragma once
// Fixed shaders used by every IMMINENCE plugin: the full-screen vertex shader, the input copy and the final
// composite (mix with the original, alpha, and the IMMINENCE watermark when the pack is locked).

static const char* kImVertex = R"IMSH(#version 410 core
layout( location = 0 ) in vec4 vPosition;
layout( location = 1 ) in vec2 vUV;
out vec2 vUv;
void main()
{
	gl_Position = vPosition;
	vUv         = vUV;
}
)IMSH";

static const char* kImCopy = R"IMSH(#version 410 core
uniform sampler2D uIn;
uniform vec2 uMaxUV;
in vec2 vUv;
out vec4 fragColor;
void main()
{
	fragColor = texture( uIn, vUv * uMaxUV );
}
)IMSH";

static const char* kImClear = R"IMSH(#version 410 core
out vec4 fragColor;
void main()
{
	fragColor = vec4( 0.0 );
}
)IMSH";

static const char* kImComposite = R"IMSH(#version 410 core
uniform sampler2D uFx;       // the effect, rendered at the chosen quality
uniform sampler2D uOrig;     // the untouched input (effects only)
uniform sampler2D uOrigB;    // second input (transitions only)
uniform vec2 uOrigMaxUV;
uniform vec2 uOrigBMaxUV;
uniform float uKind;         // 0 effect, 1 source, 2 transition
uniform float uMix;          // effects: how much of the effect is used
uniform float uProgress;     // transitions: crossfade position
uniform float uLocked;       // 1 = draw the IMMINENCE watermark
uniform float uTime;
uniform vec2 uRes;
in vec2 vUv;
out vec4 fragColor;

float seg( vec2 p, vec2 a, vec2 b )
{
	vec2 pa = p - a, ba = b - a;
	float h = clamp( dot( pa, ba ) / dot( ba, ba ), 0.0, 1.0 );
	return length( pa - ba * h );
}
// Letters drawn with line segments inside a box 1 unit tall.
float letterI( vec2 p ) { return seg( p, vec2( .1, 0. ), vec2( .1, 1. ) ); }
float letterM( vec2 p )
{
	return min( min( seg( p, vec2( .08, 0. ), vec2( .08, 1. ) ), seg( p, vec2( .08, 1. ), vec2( .41, .38 ) ) ),
				min( seg( p, vec2( .41, .38 ), vec2( .74, 1. ) ), seg( p, vec2( .74, 1. ), vec2( .74, 0. ) ) ) );
}
float letterN( vec2 p )
{
	return min( min( seg( p, vec2( .08, 0. ), vec2( .08, 1. ) ), seg( p, vec2( .08, 1. ), vec2( .62, 0. ) ) ),
				seg( p, vec2( .62, 0. ), vec2( .62, 1. ) ) );
}
float letterE( vec2 p )
{
	return min( min( seg( p, vec2( .08, 0. ), vec2( .08, 1. ) ), seg( p, vec2( .08, 1. ), vec2( .55, 1. ) ) ),
				min( seg( p, vec2( .08, .5 ), vec2( .46, .5 ) ), seg( p, vec2( .08, 0. ), vec2( .55, 0. ) ) ) );
}
float letterC( vec2 p )
{
	vec2 c  = vec2( .38, .5 );
	float r = .46;
	vec2 d  = p - c;
	float a = atan( d.y, d.x );
	if( abs( a ) < .75 )
	{
		vec2 e1 = c + r * vec2( cos( .75 ), sin( .75 ) );
		vec2 e2 = c + r * vec2( cos( .75 ), -sin( .75 ) );
		return min( length( p - e1 ), length( p - e2 ) );
	}
	return abs( length( d ) - r );
}
// Distance to the word IMMINENCE, laid out from x = 0. Total width is about 6.9 units.
float word( vec2 p )
{
	float d = 1e3;
	float x = 0.;
	d = min( d, letterI( p - vec2( x, 0. ) ) ); x += .45;
	d = min( d, letterM( p - vec2( x, 0. ) ) ); x += 1.0;
	d = min( d, letterM( p - vec2( x, 0. ) ) ); x += 1.0;
	d = min( d, letterI( p - vec2( x, 0. ) ) ); x += .45;
	d = min( d, letterN( p - vec2( x, 0. ) ) ); x += .88;
	d = min( d, letterE( p - vec2( x, 0. ) ) ); x += .8;
	d = min( d, letterN( p - vec2( x, 0. ) ) ); x += .88;
	d = min( d, letterC( p - vec2( x, 0. ) ) ); x += .95;
	d = min( d, letterE( p - vec2( x, 0. ) ) );
	return d;
}
float watermark( vec2 uv )
{
	vec2 q    = ( uv - .5 ) * vec2( uRes.x / uRes.y, 1. );
	float a   = -.42;
	q         = mat2( cos( a ), -sin( a ), sin( a ), cos( a ) ) * q;
	q.x      += uTime * .012;
	q.y      += uTime * .004;
	vec2 g    = q / .05;     // letters are 5% of the screen height
	float rowH = 3.4, colW = 10.4;
	float row = floor( g.y / rowH );
	g.x      += row * colW * .5;
	vec2 cell = vec2( mod( g.x, colW ) - 1.7, mod( g.y, rowH ) - ( rowH - 1. ) * .5 );
	float d   = word( cell );
	float aa  = max( fwidth( g.y ), 1e-3 ) * 1.2;
	return smoothstep( .085 + aa, .085 - aa, d );
}
void main()
{
	vec4 fx = texture( uFx, vUv );
	vec3 col;
	float alpha;
	if( uKind < .5 )
	{
		vec4 o = texture( uOrig, vUv * uOrigMaxUV );
		col    = mix( o.rgb, fx.rgb, uMix );
		alpha  = max( o.a, max( max( fx.r, fx.g ), fx.b ) * uMix );
	}
	else if( uKind < 1.5 )
	{
		col   = fx.rgb;
		alpha = clamp( max( max( fx.r, fx.g ), fx.b ) * 2.0, 0.0, 1.0 );
	}
	else
	{
		vec4 a = texture( uOrig, vUv * uOrigMaxUV );
		vec4 b = texture( uOrigB, vUv * uOrigBMaxUV );
		col    = fx.rgb;
		alpha  = max( mix( a.a, b.a, clamp( uProgress, 0., 1. ) ), max( max( fx.r, fx.g ), fx.b ) );
	}
	if( uLocked > .5 )
	{
		float w   = watermark( vUv );
		float lum = dot( col, vec3( .2126, .7152, .0722 ) );
		vec3 ink  = lum > .55 ? vec3( 0. ) : vec3( 1. );
		col       = mix( col, ink, w * .24 );
		alpha     = max( alpha, w * .24 );
	}
	fragColor = vec4( clamp( col, 0., 1. ), clamp( alpha, 0., 1. ) );
}
)IMSH";
