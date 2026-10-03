#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D uTexture;   // the smaller level, accumulated so far
uniform sampler2D uPrev;      // this level's own downsampled highlights
uniform vec2 uSrcTexelSize;
uniform float uRadius;
uniform float uFalloff;
uniform float uLevel;   // which octave this pass is
varying vec2 vTex;
void main() {
    vec2 t = uSrcTexelSize;
    // 3x3 tent. Straight bilinear leaves square banding that becomes obvious once
    // the levels are folded together.
    vec4 s = texture2D(uTexture, vTex + vec2(-t.x,  t.y))
           + texture2D(uTexture, vTex + vec2( 0.0,  t.y)) * 2.0
           + texture2D(uTexture, vTex + vec2( t.x,  t.y))
           + texture2D(uTexture, vTex + vec2(-t.x,  0.0)) * 2.0
           + texture2D(uTexture, vTex)                    * 4.0
           + texture2D(uTexture, vTex + vec2( t.x,  0.0)) * 2.0
           + texture2D(uTexture, vTex + vec2(-t.x, -t.y))
           + texture2D(uTexture, vTex + vec2( 0.0, -t.y)) * 2.0
           + texture2D(uTexture, vTex + vec2( t.x, -t.y));
    s *= 0.0625;

    // Radius walks continuously up the pyramid: at 0 only the tightest octave
    // survives, at 100% every one contributes fully. Falloff at 0 keeps the octaves
    // equally weighted — the physical inverse-square curve; turning it up decays the
    // wide ones and pulls the glow into a compact core.
    float w = clamp(uRadius * 6.0 - uLevel, 0.0, 1.0) * mix(1.0, 0.35, uFalloff);
    gl_FragColor = texture2D(uPrev, vTex) + s * w;
}
