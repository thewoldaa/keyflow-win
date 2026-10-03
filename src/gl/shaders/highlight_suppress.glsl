#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D uTexture;
uniform vec2 uSrcTexelSize;
uniform float uHeadroom;
uniform float uThreshold;
uniform float uSoftness;
varying vec2 vTex;
void main() {
    // 2x2 box while halving: one tap would alias thin highlights into a glow that
    // crawls and flickers as the layer moves.
    vec2 o = uSrcTexelSize * 0.5;
    vec4 c = texture2D(uTexture, vTex + vec2(-o.x, -o.y))
           + texture2D(uTexture, vTex + vec2( o.x, -o.y))
           + texture2D(uTexture, vTex + vec2(-o.x,  o.y))
           + texture2D(uTexture, vTex + vec2( o.x,  o.y));
    c *= 0.25;

    float a = c.a;
    vec3 straight = clamp(c.rgb / max(a, 1e-4), 0.0, 1.0);
    vec3 lin = pow(straight, vec3(2.2)) * a;   // premultiplied linear light

    // Threshold is measured on the GAMMA-encoded value even though the blur runs on
    // the linear one: 50 on the wheel then means the mid-grey the user sees, not
    // linear 0.5 (which is sRGB 73% — a threshold that ignores everything but the
    // brightest highlights and reads as the parameter doing nothing for half its range).
    float br = max(straight.r, max(straight.g, straight.b));
    float knee = uSoftness * 0.5;
    float mask = smoothstep(uThreshold - knee, uThreshold + knee + 1e-4, br);
    // Alpha rides along: it is what lets the glow spread OUTSIDE the layer's
    // silhouette in the final composite. uHeadroom is 1 on a float pyramid and a
    // compression factor on an 8-bit one; the composite divides it back out.
    gl_FragColor = vec4(lin * mask, a * mask) * uHeadroom;
}
