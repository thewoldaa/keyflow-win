#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D uTexture;   // the blurred quarter-size result, premultiplied linear light
uniform sampler2D uInput;     // the stack's state as this effect received it
uniform vec2 uSrcTexelSize;
uniform float uHeadroom;
uniform float uRadius;
uniform float uGamma;
uniform float uBlurAlpha;
uniform float uMixWithOriginal;
varying vec2 vTex;

vec3 shoulder(vec3 x) {
    vec3 t = max(x - 0.85, vec3(0.0));
    return min(x, vec3(0.85)) + 0.15 * (vec3(1.0) - exp(-t / 0.15));
}

void main() {

    vec2 t = uSrcTexelSize;
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

    s /= max(uHeadroom, 1e-4);
    float a = clamp(s.a, 0.0, 1.0);
    vec3 lin = max(s.rgb / max(s.a, 1e-4), vec3(0.0));
    vec3 srgb = pow(max(shoulder(lin), vec3(1e-6)), vec3(1.0 / (2.2 * uGamma)));

    vec4 base = texture2D(uInput, vTex);
    float outA = mix(base.a, a, step(0.5, uBlurAlpha));
    vec4 blurred = vec4(clamp(srgb, 0.0, 1.0) * outA, outA);
    float k = clamp(uRadius / 0.04, 0.0, 1.0) * (1.0 - uMixWithOriginal);
    gl_FragColor = mix(base, blurred, k);
}
