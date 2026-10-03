#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D uTexture;
uniform vec2 uSrcTexelSize;
varying vec2 vTex;
void main() {
    vec2 t = uSrcTexelSize;
    // The 13-tap "four squares plus a centre square" filter: a plain box filter
    // pulses as bright detail crosses a texel boundary, which on a glow reads as
    // flicker.
    vec4 a = texture2D(uTexture, vTex + vec2(-2.0 * t.x,  2.0 * t.y));
    vec4 b = texture2D(uTexture, vTex + vec2( 0.0,        2.0 * t.y));
    vec4 c = texture2D(uTexture, vTex + vec2( 2.0 * t.x,  2.0 * t.y));
    vec4 d = texture2D(uTexture, vTex + vec2(-2.0 * t.x,  0.0));
    vec4 e = texture2D(uTexture, vTex);
    vec4 f = texture2D(uTexture, vTex + vec2( 2.0 * t.x,  0.0));
    vec4 g = texture2D(uTexture, vTex + vec2(-2.0 * t.x, -2.0 * t.y));
    vec4 h = texture2D(uTexture, vTex + vec2( 0.0,       -2.0 * t.y));
    vec4 i = texture2D(uTexture, vTex + vec2( 2.0 * t.x, -2.0 * t.y));
    vec4 j = texture2D(uTexture, vTex + vec2(-t.x,  t.y));
    vec4 k = texture2D(uTexture, vTex + vec2( t.x,  t.y));
    vec4 l = texture2D(uTexture, vTex + vec2(-t.x, -t.y));
    vec4 m = texture2D(uTexture, vTex + vec2( t.x, -t.y));
    vec4 o = e * 0.125;
    o += (a + c + g + i) * 0.03125;
    o += (b + d + f + h) * 0.0625;
    o += (j + k + l + m) * 0.125;
    gl_FragColor = o;
}
