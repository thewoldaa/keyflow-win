precision mediump float;
uniform sampler2D uTexture;    // the frame AFTER the adjustment's effect stack
uniform sampler2D uOriginal;   // the same frame BEFORE it
uniform sampler2D uMask;       // the targeted layers rendered alone (premultiplied)
uniform int uUseMask;          // 0 = plain adjustment, nothing bound to uMask
uniform float uAlpha;          // the adjustment layer's opacity
uniform float uStrength;
uniform int uMode;
varying vec2 vTex;

float lum(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
vec3 clipColor(vec3 c) {
    float l = lum(c);
    float n = min(min(c.r, c.g), c.b);
    float x = max(max(c.r, c.g), c.b);
    if (n < 0.0) c = l + (c - l) * l / max(l - n, 1e-4);
    if (x > 1.0) c = l + (c - l) * (1.0 - l) / max(x - l, 1e-4);
    return c;
}
vec3 setLum(vec3 c, float l) { return clipColor(c + (l - lum(c))); }
float sat(vec3 c) { return max(max(c.r, c.g), c.b) - min(min(c.r, c.g), c.b); }
vec3 setSat(vec3 c, float s) {
    float mn = min(min(c.r, c.g), c.b);
    float mx = max(max(c.r, c.g), c.b);
    if (mx > mn) return (c - mn) * s / (mx - mn);
    return vec3(0.0);
}

vec3 blendColor(vec3 b, vec3 s) {
    if (uMode == 1) return min(b, s);                                              // Darken
    if (uMode == 2) return b * s;                                                  // Multiply
    if (uMode == 3) return 1.0 - min(vec3(1.0), (1.0 - b) / max(s, vec3(1e-4)));   // Color Burn
    if (uMode == 4) return max(b + s - 1.0, 0.0);                                  // Linear Burn
    if (uMode == 5) return max(b, s);                                              // Lighten
    if (uMode == 6) return b + s - b * s;                                          // Screen
    if (uMode == 7) return min(vec3(1.0), b / max(1.0 - s, vec3(1e-4)));           // Color Dodge
    if (uMode == 8) return min(b + s, vec3(1.0));                                  // Linear Dodge
    if (uMode == 9) return mix(2.0 * b * s, 1.0 - 2.0 * (1.0 - b) * (1.0 - s), step(vec3(0.5), b)); // Overlay
    if (uMode == 10) {                                                             // Soft Light
        vec3 d = mix(((16.0 * b - 12.0) * b + 4.0) * b, sqrt(b), step(vec3(0.25), b));
        return mix(b - (1.0 - 2.0 * s) * b * (1.0 - b), b + (2.0 * s - 1.0) * (d - b), step(vec3(0.5), s));
    }
    if (uMode == 11) return mix(2.0 * b * s, 1.0 - 2.0 * (1.0 - b) * (1.0 - s), step(vec3(0.5), s)); // Hard Light
    if (uMode == 12) {                                                             // Vivid Light
        vec3 cb = 1.0 - min(vec3(1.0), (1.0 - b) / max(2.0 * s, vec3(1e-4)));
        vec3 cd = min(vec3(1.0), b / max(2.0 - 2.0 * s, vec3(1e-4)));
        return mix(cb, cd, step(vec3(0.5), s));
    }
    if (uMode == 13) return abs(b - s);                                            // Difference
    if (uMode == 14) return b + s - 2.0 * b * s;                                   // Exclusion
    if (uMode == 15) return max(b - s, 0.0);                                       // Subtract
    if (uMode == 16) return min(b / max(s, vec3(1e-4)), vec3(1.0));                // Divide
    if (uMode == 17) return setLum(setSat(s, sat(b)), lum(b));                     // Hue
    if (uMode == 18) return setLum(setSat(b, sat(s)), lum(b));                     // Saturation
    if (uMode == 19) return setLum(s, lum(b));                                     // Color
    return setLum(b, lum(s));                                                      // Luminosity
}

void main() {
    vec4 dst = texture2D(uOriginal, vTex);
    vec4 src = texture2D(uTexture, vTex);
    if (uMode != 0 && uStrength > 0.0) {
        // Both frames are premultiplied; the blend maths needs straight colour.
        vec3 b = clamp(dst.rgb / max(dst.a, 1e-4), 0.0, 1.0);
        vec3 s = clamp(src.rgb / max(src.a, 1e-4), 0.0, 1.0);
        vec3 mixed = mix(s, blendColor(b, s), uStrength * dst.a);
        src = vec4(mixed * src.a, src.a);
    }
    float k = uAlpha;
    if (uUseMask == 1) k *= clamp(texture2D(uMask, vTex).a, 0.0, 1.0);
    gl_FragColor = mix(dst, src, clamp(k, 0.0, 1.0));
}
