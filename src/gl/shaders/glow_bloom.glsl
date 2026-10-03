#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D uTexture;   // the resolved pyramid, at half resolution
uniform sampler2D uInput;     // the stack's state as this effect received it
uniform float uHeadroom;
uniform float uExposure;
uniform float uTintR;
uniform float uTintG;
uniform float uTintB;
uniform float uTintEnabled;
uniform float uTintAmount;
uniform float uChromatic;
uniform float uAddMode;
uniform float uGlowOnly;
varying vec2 vTex;
void main() {
    // Radial channel split: reading R and B at slightly different scales about the
    // frame centre is what gives a large bloom its coloured fringe.
    vec4 g = texture2D(uTexture, vTex);
    if (uChromatic > 0.001) {
        vec2 d = vTex - 0.5;
        float ca = uChromatic * 0.04;
        g.r = texture2D(uTexture, 0.5 + d * (1.0 + ca)).r;
        g.b = texture2D(uTexture, 0.5 + d * (1.0 - ca)).b;
    }
    // Exposure multiplies the thresholded source. Applied here rather than in the
    // prefilter only so it cannot overflow an 8-bit pyramid — a blur is linear, so
    // scaling before or after it is the same light either way.
    vec3 light = g.rgb * (uExposure / max(uHeadroom, 1e-4));
    // Tint, gated by its own switch so the swatch is the only thing that decides
    // whether the glow takes a colour. Multiplicative: it can only take light OUT of
    // a channel, which is what keeps a tinted glow inside the same exposure as an
    // untinted one — a tint that brightened would double as a second Exposure.
    if (uTintEnabled > 0.5) {
        light *= mix(vec3(1.0), vec3(uTintR, uTintG, uTintB), uTintAmount);
    }
    light = max(light, vec3(0.0));

    // TONEMAP, never clamp. Six summed octaves run well past white near a bright
    // source; clipping them leaves a flat plateau bounded by a hard rim exactly
    // where the sum crosses 1.0 — a visible contour sitting in the middle of what
    // should be a smooth falloff. 1-exp(-x) is the identity for the faint tail, so
    // the halo keeps every bit of its reach, and eases the core in asymptotically.
    // This is the job Deep Glow's tonemapping curves do.
    vec3 toned = (uAddMode > 0.5) ? min(light, vec3(1.0)) : (vec3(1.0) - exp(-light));
    vec3 glowG = pow(toned, vec3(1.0 / 2.2));   // gamma-encoded light

    vec4 base = texture2D(uInput, vTex);   // premultiplied, gamma-encoded
    if (uGlowOnly > 0.5) base = vec4(0.0);

    // Composite the glow as its own premultiplied source over the input, in the
    // space the target is blended in. Its coverage is its own brightness: that is
    // the SMALLEST alpha able to carry the light, so the backdrop stays as visible
    // as it can. Deriving alpha any other way — a max() of two curves, say — puts a
    // crease in the falloff where the two cross, and an alpha larger than the light
    // it carries makes the far tail darken the backdrop instead of lighting it.
    float cov = max(glowG.r, max(glowG.g, glowG.b));
    vec3 outRgb = (uAddMode > 0.5)
        ? min(base.rgb + glowG, vec3(1.0))
        : base.rgb + glowG - base.rgb * glowG;   // screen
    float outA = clamp(
        max(base.a + cov * (1.0 - base.a), max(outRgb.r, max(outRgb.g, outRgb.b))),
        0.0, 1.0
    );
    // Sub-LSB hash dither, against the banding a shallow falloff shows at 8 bits.
    // Clamping to outA keeps the result a valid premultiplied colour and confines
    // the noise to where the glow actually reaches.
    float d = fract(sin(dot(vTex, vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
    outRgb += d / 255.0;
    gl_FragColor = vec4(clamp(outRgb, 0.0, outA), outA);
}
