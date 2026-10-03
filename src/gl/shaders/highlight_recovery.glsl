#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D uTexture;
uniform vec2 uSrcTexelSize;
uniform float uHeadroom;
uniform float uGamma;
uniform float uHighlightMode;
uniform float uHighlightBoost;
uniform float uHighlightThreshold;
uniform float uSuppressThreshold;
uniform float uBoostSoften;
varying vec2 vTex;

// A soft step of width `soften` about `edge`. The half-width is floored so a soften
// of 0 is a hard cut rather than smoothstep with equal edges (undefined in GLSL).
float knee(float edge, float soften, float x) {
    float k = max(soften * 0.5, 1e-4);
    return smoothstep(edge - k, edge + k, x);
}

void main() {
    // 2x2 box while halving: one tap would alias thin highlights into a bokeh that
    // crawls and flickers as the layer moves.
    vec2 o = uSrcTexelSize * 0.5;
    vec4 c = texture2D(uTexture, vTex + vec2(-o.x, -o.y))
           + texture2D(uTexture, vTex + vec2( o.x, -o.y))
           + texture2D(uTexture, vTex + vec2(-o.x,  o.y))
           + texture2D(uTexture, vTex + vec2( o.x,  o.y));
    c *= 0.25;

    float a = c.a;
    vec3 straight = clamp(c.rgb / max(a, 1e-4), 0.0, 1.0);
    // Gamma is an exponent on top of the display curve: 1 is physically linear
    // light, above it the highlights take a still larger share of the blur.
    // pow(0, y) is undefined in GLSL ES, hence the floor.
    vec3 lin = pow(max(straight, vec3(1e-4)), vec3(2.2 * uGamma));
    float luma = dot(straight, vec3(0.2126, 0.7152, 0.0722));

    // Boost is quadratic on the dial — fine control near zero, up to x41 at the
    // top, which is what a small light needs to survive being spread over a disc.
    float b = uHighlightBoost * uHighlightBoost * 40.0;
    int mode = int(uHighlightMode + 0.5);
    if (mode == 0) {
        // Luma: the colour scaled up as a whole, hue kept.
        lin *= 1.0 + b * knee(uHighlightThreshold, uBoostSoften, luma);
    } else if (mode == 1) {
        // Luma Boost to White: white light ADDED, so a boosted highlight blows out
        // toward white the way a sensor does.
        lin += vec3(b * knee(uHighlightThreshold, uBoostSoften, luma));
    } else if (mode == 2) {
        // RGB Glow: each channel on its own, so a saturated light glows in its colour.
        lin *= 1.0 + b * vec3(
            knee(uHighlightThreshold, uBoostSoften, straight.r),
            knee(uHighlightThreshold, uBoostSoften, straight.g),
            knee(uHighlightThreshold, uBoostSoften, straight.b)
        );
    } else {
        // RGB Max: measured on the brightest channel, scaled as a whole.
        float mx = max(straight.r, max(straight.g, straight.b));
        lin *= 1.0 + b * knee(uHighlightThreshold, uBoostSoften, mx);
    }
    // Suppress: what sits under its threshold is darkened, by up to the threshold.
    lin *= 1.0 - uSuppressThreshold * (1.0 - knee(uSuppressThreshold, uBoostSoften, luma));

    // Premultiplied linear light. uHeadroom is 1 on a float target and a compression
    // factor on the 8-bit fallback; the composite divides it back out.
    gl_FragColor = vec4(lin * a, a) * uHeadroom;
}
