// ---------------------------------------------------------------------------
// The shader table.
//
// GENERATED from the recovered shader set. Do not edit by hand: edit the
// .glsl files under src/gl/shaders and re-run scripts/embed-shaders.ps1, which
// regenerates this file. A hand edit here survives until the next build and
// then disappears, which is the worst of both.
//
// The shaders are GLSL ES 1.00, recovered from the Android build and adapted
// for a desktop context by gl/GlslCompat.cpp at compile time. They are
// compiled into the executable rather than read from disk so the app cannot
// run against a shader set that does not match its own catalogue.
//
// Long units are emitted as several adjacent raw string literals. MSVC caps a
// single literal at 16380 bytes; adjacent literals are concatenated at compile
// time and produce the same string.
// ---------------------------------------------------------------------------

#include "gl/ShaderTable.h"

#include <array>
#include <string_view>

namespace keyflow {
namespace {

struct Entry
{
    std::string_view stem;
    std::string_view source;
};

constexpr std::array<Entry, 63> kShaders = { {
    { "adjustment_layer",
R"KF(precision mediump float;
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

)KF"
    },
    { "alpha_copy",
R"KF(precision mediump float;
uniform sampler2D uTexture;
uniform float uAlpha;
varying vec2 vTex;
void main() {
    vec4 c = texture2D(uTexture, vTex);
    gl_FragColor = vec4(c.rgb * uAlpha, c.a * uAlpha);
}

)KF"
    },
    { "audio_spectrum",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform sampler2D uSpectrum;
        uniform vec2 uTexelSize;
        uniform mat3 uLayerToLocal;
        uniform float uBands;
        uniform float uStartX;
        uniform float uStartY;
        uniform float uEndX;
        uniform float uEndY;
        uniform float uPolar;
        uniform float uMaxHeight;
        uniform float uThickness;
        uniform float uSoftness;
        uniform float uDisplay;
        uniform float uSide;
        uniform float uInR;
        uniform float uInG;
        uniform float uInB;
        uniform float uOutR;
        uniform float uOutG;
        uniform float uOutB;
        uniform float uHueInterp;
        uniform float uDynamicHue;
        uniform float uColorSymmetry;
        uniform float uCompositeOnOriginal;
        varying vec2 vTex;


float lineCoverage(float d, float hw, float w) {
    w = max(w, 1e-6);
    float lo = max(d - 0.5 * w, -hw);
    float hi = min(d + 0.5 * w, hw);
    return clamp((hi - lo) / w, 0.0, 1.0);
}


        // Band i's magnitude, 0..1. Sixteen bits split over R (high) and G (low) —
        // see AudioSeries; the texture is NEAREST-filtered so both bytes come from
        // the one texel. Outside the row (i < 0, i >= n) there is no band: silence.
        float bandAt(float i, float n) {
            if (i < 0.0 || i >= n) return 0.0;
            vec4 s = texture2D(uSpectrum, vec2((i + 0.5) / 256.0, 0.5));
            return (s.r * 256.0 + s.g) / 257.0;
        }

        // Turn a colour's hue by deg degrees — a rotation in YIQ, like Hue/Saturation.
        vec3 hueRotate(vec3 c, float deg) {
            float a = radians(deg);
            float cs = cos(a);
            float sn = sin(a);
            float y = dot(c, vec3(0.299, 0.587, 0.114));
            float i = dot(c, vec3(0.596, -0.274, -0.322));
            float q = dot(c, vec3(0.211, -0.523, 0.312));
            float i2 = i * cs - q * sn;
            float q2 = i * sn + q * cs;
            return vec3(
                y + 0.956 * i2 + 0.621 * q2,
                y - 0.272 * i2 - 0.647 * q2,
                y - 1.106 * i2 + 1.703 * q2
            );
        }

        void main() {
            vec4 base = texture2D(uTexture, vTex);

vec3 q = uLayerToLocal * vec3(vTex, 1.0);
if (abs(q.z) < 0.001) {
    gl_FragColor = base;
    return;
}
float s = q.x / q.z;
float t = q.y / q.z;
vec2 row0 = vec2(uLayerToLocal[0][0], uLayerToLocal[1][0]);
vec2 row1 = vec2(uLayerToLocal[0][1], uLayerToLocal[1][1]);
vec2 row2 = vec2(uLayerToLocal[0][2], uLayerToLocal[1][2]);
// One screen pixel, in s and in t, at THIS fragment.
float pxS = length((row0 - s * row2) / q.z * uTexelSize);
float pxT = length((row1 - t * row2) / q.z * uTexelSize);
// The layer's size on screen, in pixels, at its centre (w == 1 there).
float layerW = 1.0 / max(length((row0 - 0.5 * row2) * uTexelSize), 1e-6);
float layerH = 1.0 / max(length((row1 - 0.5 * row2) * uTexelSize), 1e-6);
float layerAspect = layerW / layerH;
vec2 p = vec2(s * layerAspect, t);
float px = max(pxS * layerAspect, pxT);
// One composition pixel at 1080p (the convention Pixelate and Find Edges use), in
// pattern units: frame height over layer height, both as they stand on THIS target,
// so a "1 px" border is the same picture at preview resolution and in the export,
// and stays 1 px when the layer is scaled.
float pxUnit = 1.0 / (uTexelSize.y * layerH * 1080.0);

float hS = 0.5 * pxS;
float hT = 0.5 * pxT;
float insideQuad = smoothstep(-hS, hS, s) * (1.0 - smoothstep(1.0 - hS, 1.0 + hS, s))
                 * smoothstep(-hT, hT, t) * (1.0 - smoothstep(1.0 - hT, 1.0 + hT, t));

float inside = insideQuad;
if (inside <= 0.0) {
    gl_FragColor = base;
    return;
}

            float n = max(floor(uBands + 0.5), 1.0);
            vec2 P0 = vec2(uStartX * layerAspect, uStartY);
            vec2 P1 = vec2(uEndX * layerAspect, uEndY);
            // The path in this fragment's terms: u runs 0..1 from start to end, v is
            // the signed distance from it (side A positive), along is the path's
            // length per unit of u HERE — the line's length, or the circumference at
            // this radius — so a bar's width is in the same units as its height.
            float u;
            float v;
            float along;
            if (uPolar < 0.5) {
                vec2 d = P1 - P0;
                float L = max(length(d), 1e-5);
                vec2 dir = d / L;
                vec2 rel = p - P0;
                u = dot(rel, dir) / L;
                v = dot(rel, vec2(dir.y, -dir.x));
                along = L;
            } else {
                // Polar: the start point is the centre, the end point sets the
                // radius and where band 0 begins; bands go round clockwise and
                // side A is outward.
                vec2 rel = p - P0;
                float r = length(rel);
                float r0 = max(length(P1 - P0), 1e-5);
                float a0 = atan(P1.y - P0.y, P1.x - P0.x);
                u = fract((atan(rel.y, rel.x) - a0) / 6.2831853);
                v = r - r0;
                along = 6.2831853 * max(r, 1e-5);
            }
            int side = int(uSide + 0.5);
            float vs = side == 0 ? v : (side == 1 ? -v : abs(v));
            float maxH = max(uMaxHeight, 1e-5);
            float th = max(uThickness * pxUnit, 0.0);
            float hw = 0.5 * th;
            // Every edge is one screen pixel wide, plus Softness worth of the bar.
            float aa = px + uSoftness * th;
            int mode = int(uDisplay + 0.5);
            float fi = floor(u * n);
            float seg = along / n;
            float cov = 0.0;
            if (mode == 0) {
                // Digital: a bar per band, Thickness wide, standing on the path.
                float uc = (fi + 0.5) / n;
                float dAlong = abs(u - uc) * along;
                float H = bandAt(fi, n) * maxH;
                cov = lineCoverage(dAlong, hw, aa) * lineCoverage(vs - 0.5 * H, 0.5 * H, aa);
            } else if (mode == 1) {
                // Analog lines: the polyline through the band tops, falling to the
                // path at either end. Distance to the segment, not just the vertical
                // gap, so a steep rise is as thin as a flat run.
                float x = u * n - 0.5;
                float i0 = floor(x);
                float f = x - i0;
                float H0 = bandAt(i0, n) * maxH;
                float H1 = bandAt(i0 + 1.0, n) * maxH;
                float Hu = mix(H0, H1, f);
                float slope = (H1 - H0) / max(seg, 1e-6);
                float dist = abs(vs - Hu) / sqrt(1.0 + slope * slope);
                cov = lineCoverage(dist, hw, aa) * step(0.0, u) * step(u, 1.0);
            } else {
                // Analog dots: a disc at each band's top. A dot wider than its slot
)KF"
R"KF(                // reaches into the neighbours', so the three nearest are tried.
                for (int k = -1; k <= 1; k++) {
                    float i = fi + float(k);
                    if (i < 0.0 || i >= n) continue;
                    float uc = (i + 0.5) / n;
                    float Hi = bandAt(i, n) * maxH;
                    vec2 dd = vec2((u - uc) * along, vs - Hi);
                    cov = max(cov, lineCoverage(length(dd), hw, aa));
                }
            }
            // Inside colour at the path, Outside at Maximum Height, and the hue turned
            // on the way — there and back with Color Symmetry, further by the frame's
            // loudness (the row's B channel) with Dynamic Hue Phase.
            float fr = clamp(vs / maxH, 0.0, 1.0);
            vec3 col = mix(vec3(uInR, uInG, uInB), vec3(uOutR, uOutG, uOutB), fr);
            float ph = uColorSymmetry > 0.5 ? 1.0 - abs(2.0 * fr - 1.0) : fr;
            float level = texture2D(uSpectrum, vec2(0.5 / 256.0, 0.5)).b;
            float shift = uHueInterp * (ph + (uDynamicHue > 0.5 ? level : 0.0));
            if (uHueInterp > 0.0) col = hueRotate(col, shift);
            vec4 paint = vec4(clamp(col, 0.0, 1.0), 1.0) * cov;
            // Off (AE's default) the layer's own pixels are gone and only the
            // spectrum remains; on, it is painted over them.
            vec4 result = uCompositeOnOriginal > 0.5 ? paint + base * (1.0 - cov) : paint;
            gl_FragColor = mix(base, result, inside);
        }

)KF"
    },
    { "blend_backdrop",
R"KF(precision mediump float;
uniform sampler2D uTexture;
        uniform sampler2D uBackdrop;
        uniform float uAlpha;
        uniform float uStrength;
        uniform int uMode;
        uniform vec2 uTargetSize;
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
            vec4 src = texture2D(uTexture, vTex);
            src.rgb /= max(src.a, 1e-4);
            vec4 dst = texture2D(uBackdrop, gl_FragCoord.xy / uTargetSize);
            // The snapshot is premultiplied; the blend maths needs straight colour.
            vec3 b = clamp(dst.rgb / max(dst.a, 1e-4), 0.0, 1.0);
            vec3 s = clamp(src.rgb, 0.0, 1.0);
            // Weighted by the backdrop's own alpha: with nothing underneath there is nothing
            // to blend WITH, so the source composites plainly instead of against black.
            vec3 mixed = mix(s, blendColor(b, s), uStrength * dst.a);
            float a = clamp(src.a * uAlpha, 0.0, 1.0);
            // Src-over, premultiplied out (dst.rgb already is).
            gl_FragColor = vec4(mixed * a + dst.rgb * (1.0 - a), a + dst.a * (1.0 - a));
        }

)KF"
    },
    { "blur_13tap",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
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

)KF"
    },
    { "blur_masked",
R"KF(precision mediump float;
uniform sampler2D uTexture;   // the blurred quarter-size result
uniform sampler2D uInput;     // the stack's state as this effect received it
uniform vec2 uSrcTexelSize;
uniform float uRadius;
varying vec2 vTex;
void main() {
    vec2 t = uSrcTexelSize;
    // 3x3 tent, like the glow's upsample: plain bilinear from a quarter-size source
    // leaves square banding that is obvious across a smooth gradient.
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
    float k = clamp(uRadius / 0.04, 0.0, 1.0);
    gl_FragColor = mix(texture2D(uInput, vTex), s, k);
}

)KF"
    },
    { "blur_pyramid_down",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
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

)KF"
    },
    { "blur_pyramid_up",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
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

)KF"
    },
    { "blur_simple_x",
R"KF(precision mediump float;
uniform sampler2D uTexture;
uniform vec2 uTexelSize;
uniform vec4 uLayerRect;
uniform float uRadius;
uniform float uRepeatEdge;
varying vec2 vTex;
const int K = 10;
vec4 tap(vec2 uv, float hold) {
    vec2 held = clamp(uv, uLayerRect.xy, uLayerRect.zw);
    return texture2D(uTexture, mix(uv, held, hold));
}
void main() {
    float aspect = uTexelSize.y / uTexelSize.x;   // width / height
    float hold = step(0.5, uRepeatEdge);
    // Radius is in frame-height units; the horizontal axis divides by the aspect so
    // a blur stays round rather than stretching on a wide composition.
    vec2 dir = vec2(1.0, 0.0);
    float span = (uRadius * 0.12) / float(K);
    vec4 sum = vec4(0.0);
    float wsum = 0.0;
    for (int i = -K; i <= K; i++) {
        float w = exp(-3.0 * float(i * i) / float(K * K));
        sum += tap(vTex + dir * (float(i) * span), hold) * w;
        wsum += w;
    }
    // With the edge held, nothing is drawn past the layer's box: a one-texel ramp
    // (in texels of this target, so both axes are equally soft on any aspect) rather
    // than a step, so the held edge is antialiased.
    vec2 d = min(vTex - uLayerRect.xy, uLayerRect.zw - vTex) / max(uTexelSize, vec2(1e-6));
    float inside = clamp(min(d.x, d.y) + 0.5, 0.0, 1.0);
    gl_FragColor = sum / wsum * mix(1.0, inside, hold);
}

)KF"
    },
    { "blur_simple_y",
R"KF(precision mediump float;
uniform sampler2D uTexture;
uniform vec2 uTexelSize;
uniform vec4 uLayerRect;
uniform float uRadius;
uniform float uRepeatEdge;
varying vec2 vTex;
const int K = 10;
vec4 tap(vec2 uv, float hold) {
    vec2 held = clamp(uv, uLayerRect.xy, uLayerRect.zw);
    return texture2D(uTexture, mix(uv, held, hold));
}
void main() {
    float aspect = uTexelSize.y / uTexelSize.x;   // width / height
    float hold = step(0.5, uRepeatEdge);
    // Radius is in frame-height units; the horizontal axis divides by the aspect so
    // a blur stays round rather than stretching on a wide composition.
    vec2 dir = vec2(0.0, 1.0);
    float span = (uRadius * 0.12) / float(K);
    vec4 sum = vec4(0.0);
    float wsum = 0.0;
    for (int i = -K; i <= K; i++) {
        float w = exp(-3.0 * float(i * i) / float(K * K));
        sum += tap(vTex + dir * (float(i) * span), hold) * w;
        wsum += w;
    }
    // With the edge held, nothing is drawn past the layer's box: a one-texel ramp
    // (in texels of this target, so both axes are equally soft on any aspect) rather
    // than a step, so the held edge is antialiased.
    vec2 d = min(vTex - uLayerRect.xy, uLayerRect.zw - vTex) / max(uTexelSize, vec2(1e-6));
    float inside = clamp(min(d.x, d.y) + 0.5, 0.0, 1.0);
    gl_FragColor = sum / wsum * mix(1.0, inside, hold);
}

)KF"
    },
    { "bokeh_iris_blur",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D uTexture;
uniform vec2 uTexelSize;
uniform float uRadius;
uniform float uScaleX;
uniform float uScaleY;
uniform float uIrisShape;          // index into the shape list — see the spec
uniform float uIrisCurvature;      // + toward a circle, - toward a star
uniform float uRotation;           // degrees
uniform float uBokeh;              // 1 = flat disc, 0 = Gaussian-weighted
uniform float uBokehShading;       // + rim, - centre

uniform vec4 uLayerRect;
uniform float uRepeatEdge;
vec4 tap(vec2 uv, float hold) {
    vec2 held = clamp(uv, uLayerRect.xy, uLayerRect.zw);
    return texture2D(uTexture, mix(uv, held, hold));
}

varying vec2 vTex;
const int N = 64;
const float GOLDEN = 2.39996323;

// How far the aperture reaches in direction `phi`, 0..1: the polygon's edge (a
// vertex at 1), bent by the curvature; 1 everywhere for a circle.
float iris(float phi, float sides, float curve) {
    if (sides < 2.5) return 1.0;
    float sector = 6.28318530 / sides;
    float a = mod(phi, sector) - sector * 0.5;
    float poly = min(cos(sector * 0.5) / max(cos(a), 1e-3), 1.0);
    // Positive: the blades bow out toward the circle. Negative: the edge midpoints
    // are pulled in toward the centre, a star (poly is at least cos(sector/2) > 0,
    // so the pow has a safe base).
    return curve >= 0.0 ? mix(poly, 1.0, curve) : pow(poly, 1.0 + 4.0 * (-curve));
}

void main() {
    float aspect = uTexelSize.y / uTexelSize.x;   // width / height
    float hold = step(0.5, uRepeatEdge);
    float R = uRadius * 0.12;         // frame-height units
    // Circle, then 3 … 16 sides.
    int shape = int(uIrisShape + 0.5);
    float sides = shape == 0 ? 0.0 : float(shape + 2);
    float rot = radians(uRotation);
    vec2 stretch = vec2(uScaleX, uScaleY);
    vec4 sum = vec4(0.0);
    float wsum = 0.0;
    for (int i = 0; i < N; i++) {
        float phi = float(i) * GOLDEN;
        float r0 = sqrt((float(i) + 0.5) / float(N));
        float r = r0 * iris(phi + rot, sides, uIrisCurvature);
        vec2 o = vec2(cos(phi), sin(phi)) * (r * R) * stretch;
        vec4 s = tap(vTex + vec2(o.x / aspect, o.y), hold);
        float w = mix(exp(-2.5 * r0 * r0), 1.0, uBokeh);
        w *= max(1.0 + uBokehShading * (2.0 * r0 * r0 - 1.0), 0.0);
        sum += s * w;
        wsum += w;
    }
    gl_FragColor = sum / max(wsum, 1e-4);
}

)KF"
    },
    { "box_blur",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D uTexture;
uniform vec2 uTexelSize;
uniform float uRadius;
uniform float uScaleX;
uniform float uScaleY;

uniform vec4 uLayerRect;
uniform float uRepeatEdge;
vec4 tap(vec2 uv, float hold) {
    vec2 held = clamp(uv, uLayerRect.xy, uLayerRect.zw);
    return texture2D(uTexture, mix(uv, held, hold));
}

varying vec2 vTex;
const int N = 12;
const float GOLDEN = 2.39996323;
void main() {
    float aspect = uTexelSize.y / uTexelSize.x;   // width / height
    float hold = step(0.5, uRepeatEdge);
    float R = uRadius * 0.12 * 0.16;
    vec2 stretch = vec2(uScaleX, uScaleY);
    vec4 sum = vec4(0.0);
    for (int i = 0; i < N; i++) {
        float phi = float(i) * GOLDEN;
        float r = sqrt((float(i) + 0.5) / float(N));
        vec2 o = vec2(cos(phi), sin(phi)) * (r * R) * stretch;
        sum += tap(vTex + vec2(o.x / aspect, o.y), hold);
    }
    vec2 d = min(vTex - uLayerRect.xy, uLayerRect.zw - vTex) / max(uTexelSize, vec2(1e-6));
    float inside = clamp(min(d.x, d.y) + 0.5, 0.0, 1.0);
    gl_FragColor = sum / float(N) * mix(1.0, inside, hold);
}

)KF"
    },
    { "brightness_contrast",
R"KF(precision mediump float;
uniform sampler2D uTexture;
uniform float uBrightness;
uniform float uContrast;
varying vec2 vTex;
void main() {
    vec4 c = texture2D(uTexture, vTex);
    // The layer arrives premultiplied; colour math needs straight RGB.
    float a = c.a;
    vec3 rgb = c.rgb / max(a, 1e-4);
    rgb += uBrightness;
    rgb = (rgb - 0.5) * (uContrast + 1.0) + 0.5;
    rgb = clamp(rgb, 0.0, 1.0);
    gl_FragColor = vec4(rgb * a, a);
}

)KF"
    },
    { "chroma_key",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D uTexture;
uniform vec2 uTexelSize;   // 1/w, 1/h of the target
uniform vec4 uLayerRect;   // the layer's box in target uv
uniform float uKeyR;
uniform float uKeyG;
uniform float uKeyB;
uniform float uTolerance;
uniform float uSoftness;
uniform float uPreBlur;
uniform float uClipBlack;
uniform float uClipWhite;
uniform float uSpill;
uniform float uSpillBias;
varying vec2 vTex;

vec3 ycc(vec3 c) {
    float y = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return vec3(y, (c.b - y) / 1.8556, (c.r - y) / 1.5748);
}
vec2 chroma(vec3 c) {
    float v = max(max(c.r, c.g), max(c.b, 1e-4));
    return ycc(c / v).yz;
}


void main() {
    vec4 base = texture2D(uTexture, vTex);
    if (base.a <= 0.0) {
        gl_FragColor = vec4(0.0);
        return;
    }
    vec3 p = base.rgb / base.a;
    vec3 K = vec3(uKeyR, uKeyG, uKeyB);

    // The colour the decision is made on: a ring of taps, alpha-weighted (summing
    // premultiplied colour and dividing by summed alpha IS the weighted mean), the
    // centre counted twice. Held inside the layer's box like every neighbour read.
    vec3 judged = p;
    if (uPreBlur > 0.0) {
        float rb = uPreBlur * (1.0 / 1080.0) / uTexelSize.y;   // texels
        vec4 acc = base * 2.0;
        for (int i = 0; i < 8; i++) {
            float ang = float(i) * 0.7853982;
            vec2 o = vec2(cos(ang), sin(ang)) * rb * uTexelSize;
            acc += texture2D(uTexture, clamp(vTex + o, uLayerRect.xy, uLayerRect.zw));
        }
        judged = acc.rgb / max(acc.a, 1e-4);
    }

    vec2 kc = chroma(K);
    float d = distance(chroma(judged), kc);
    // Keyed out inside the tolerance, kept beyond tolerance + softness, a smooth
    // ramp between. The floor keeps smoothstep's two edges apart at Softness 0,
    // where they would otherwise coincide and the result be undefined.
    float matte = smoothstep(uTolerance, uTolerance + max(uSoftness, 1e-4), d);
    matte = clamp((matte - uClipBlack) / max(uClipWhite - uClipBlack, 1e-4), 0.0, 1.0);
    if (matte <= 0.0) {
        gl_FragColor = vec4(0.0);
        return;
    }

    // The key's dominant channel — the one the screen lights up and spill leaks into.
    vec3 domSel = (K.g >= K.r && K.g >= K.b) ? vec3(0.0, 1.0, 0.0)
                : ((K.b >= K.r) ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0));

    // Un-mix: the foreground, premultiplied by the matte.
    float sAmt = 1.0 - matte;
    float screenScale = min(1.0, dot(p, domSel) / max(sAmt * dot(K, domSel), 1e-4));
    vec3 c = max(p - sAmt * K * screenScale, 0.0);

    // Spill: limit the dominant channel to a blend of the other two, luma restored.
    float dom = dot(c, domSel);
    vec3 others = c - dom * domSel;
    float oMax = max(max(others.r, others.g), others.b);
    vec3 oo = others + domSel * 1e9;
    float oMin = min(min(oo.r, oo.g), oo.b);
    float lim = mix(oMin, oMax, uSpillBias);
    // At an edge the un-mix can take out MORE green than the pixel's real share of
    // screen (the key is the screen at its brightest), which leaves the other two
    // channels standing: a magenta rim. Green is let back up to the spill limit —
    // never past the pixel's own green at this coverage, so nothing is invented.
    if (matte < 1.0) dom = max(dom, min(lim, dot(p, domSel) * matte));
    float excess = max(dom - lim, 0.0) * uSpill;
    float lost = excess * dot(domSel, vec3(0.2126, 0.7152, 0.0722));
    c = others + domSel * (dom - excess) + vec3(lost);
    c = min(c, vec3(matte));

    float a = base.a * matte;
    gl_FragColor = vec4(c * base.a, a);
}

)KF"
    },
    { "chromatic_aberration",
R"KF(precision mediump float;
uniform sampler2D uTexture;
uniform vec2 uTexelSize;   // 1/w, 1/h
uniform vec4 uLayerRect;
uniform float uSpread;
uniform float uShift;
uniform float uAngle;
varying vec2 vTex;
void main() {
    float aspect = uTexelSize.y / uTexelSize.x;   // width / height
    vec2 centre = (uLayerRect.xy + uLayerRect.zw) * 0.5;
    // Radial term: a scale about the centre, so it is already isotropic —
    // no aspect correction, unlike the flat shift below.
    vec2 radial = (vTex - centre) * (uSpread * 0.15);
    float ang = radians(uAngle);
    // Measured in frame HEIGHT units so the same number moves the same
    // distance whichever way the angle points.
    vec2 slide = vec2(cos(ang) / aspect, sin(ang)) * (uShift * 0.05);
    vec2 off = radial + slide;

    vec4 cr = texture2D(uTexture, vTex + off);
    vec4 cg = texture2D(uTexture, vTex);
    vec4 cb = texture2D(uTexture, vTex - off);
    // Each channel is unpremultiplied against ITS OWN sample's alpha: the
    // three taps land on pixels with different coverage, and dividing them
    // all by one alpha would pull the fringe toward black at every edge —
    // which is exactly the edge the effect exists to colour.
    vec3 straight = vec3(
        cr.r / max(cr.a, 1e-4),
        cg.g / max(cg.a, 1e-4),
        cb.b / max(cb.a, 1e-4)
    );
    // Coverage is the average of the three, so the silhouette splits too —
    // a real lens fringes the outline, not only the colour inside it.
    float a = (cr.a + cg.a + cb.a) / 3.0;
    gl_FragColor = vec4(clamp(straight, 0.0, 1.0) * a, a);
}

)KF"
    },
    { "clear",
R"KF(precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D uTexture;
void main() {
    gl_FragColor = texture2D(uTexture, vTexCoord);
}

)KF"
    },
    { "compositor_blend",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D uTexture;
uniform sampler2D uBackdrop;
uniform float uAlpha;
uniform float uStrength;
uniform int uMode;
uniform vec2 uTargetSize;
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
    // Normal is the source unchanged. It needs its own branch: without one the
    // dispatch falls through to the final return, which is Luminosity — so a
    // layer set to Normal would composite its brightness from itself and its
    // colour from the backdrop.
    if (uMode == 0) return s;
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
    vec4 src = texture2D(uTexture, vTex);

    vec4 dst = texture2D(uBackdrop, gl_FragCoord.xy / uTargetSize);
    // The snapshot is premultiplied; the blend maths needs straight colour.
    vec3 b = clamp(dst.rgb / max(dst.a, 1e-4), 0.0, 1.0);
    vec3 s = clamp(src.rgb, 0.0, 1.0);
    // Weighted by the backdrop's own alpha: with nothing underneath there is nothing
    // to blend WITH, so the source composites plainly instead of against black.
    vec3 mixed = mix(s, blendColor(b, s), uStrength * dst.a);
    float a = clamp(src.a * uAlpha, 0.0, 1.0);
    // Src-over, premultiplied out (dst.rgb already is).
    gl_FragColor = vec4(mixed * a + dst.rgb * (1.0 - a), a + dst.a * (1.0 - a));
}

)KF"
    },
    { "drop_shadow",
R"KF(precision mediump float;
uniform sampler2D uTexture;   // pass-1 output (alpha in .a)
uniform sampler2D uInput;     // the stack's state as this effect received it
uniform vec2 uTexelSize;
uniform float uShadowR;
uniform float uShadowG;
uniform float uShadowB;
uniform float uOpacity;
uniform float uSoftness;
varying vec2 vTex;
const int K = 8;
void main() {
    float step = max(uSoftness, 1e-4) / float(K);
    float sum = 0.0;
    float wsum = 0.0;
    for (int i = -K; i <= K; i++) {
        float w = exp(-3.0 * float(i * i) / float(K * K));
        sum += texture2D(uTexture, vTex + vec2(0.0, float(i) * step)).a * w;
        wsum += w;
    }
    float shA = (sum / wsum) * clamp(uOpacity, 0.0, 1.0);
    vec4 shadow = vec4(vec3(uShadowR, uShadowG, uShadowB) * shA, shA);
    vec4 layer = texture2D(uInput, vTex);   // premultiplied
    gl_FragColor = layer + shadow * (1.0 - layer.a);
}

)KF"
    },
    { "edge_detect",
R"KF(precision mediump float;
uniform sampler2D uTexture;   // original layer
uniform vec2 uTexelSize;
uniform float uWidth;
varying vec2 vTex;
const int K = 8;
void main() {
    float aspect = uTexelSize.y / uTexelSize.x;
    float step = uWidth / float(K) / aspect;
    float m = 0.0;
    for (int i = -K; i <= K; i++) {
        m = max(m, texture2D(uTexture, vTex + vec2(float(i) * step, 0.0)).a);
    }
    gl_FragColor = vec4(0.0, 0.0, 0.0, m);
}

)KF"
    },
    { "edge_outline",
R"KF(precision mediump float;
uniform sampler2D uTexture;   // original layer
uniform vec2 uTexelSize;
uniform float uDirection;
uniform float uDistance;
uniform float uSoftness;
varying vec2 vTex;
const int K = 8;
void main() {
    float aspect = uTexelSize.y / uTexelSize.x;
    float ang = radians(uDirection);
    vec2 off = vec2(cos(ang) / aspect, sin(ang)) * uDistance;
    float step = max(uSoftness, 1e-4) / float(K) / aspect;
    float sum = 0.0;
    float wsum = 0.0;
    for (int i = -K; i <= K; i++) {
        float w = exp(-3.0 * float(i * i) / float(K * K));
        sum += texture2D(uTexture, vTex - off + vec2(float(i) * step, 0.0)).a * w;
        wsum += w;
    }
    gl_FragColor = vec4(0.0, 0.0, 0.0, sum / wsum);
}

)KF"
    },
    { "exposure_gamma",
R"KF(        precision mediump float;
        uniform sampler2D uTexture;
        uniform float uExposure;
        uniform float uOffset;
        uniform float uGamma;
        varying vec2 vTex;
        void main() {

vec4 src = texture2D(uTexture, vTex);
float a = src.a;
vec3 rgb = clamp(src.rgb / max(a, 1e-4), 0.0, 1.0);

            vec3 v = rgb * pow(2.0, uExposure) + uOffset;
            // max() before pow: pow() of a negative base is undefined in GLSL, and
            // a negative offset takes dark pixels below zero as a matter of course.
            v = pow(max(v, vec3(0.0)), vec3(1.0 / max(uGamma, 1e-3)));
            v = clamp(v, 0.0, 1.0);
            gl_FragColor = vec4(v * a, a);
        }

)KF"
    },
    { "feather_mask",
R"KF(precision mediump float;
uniform sampler2D uTexture;
uniform vec2 uSrcTexelSize;
varying vec2 vTex;
void main() {
    vec2 o = uSrcTexelSize * 0.5;
    vec4 c = texture2D(uTexture, vTex + vec2(-o.x, -o.y))
           + texture2D(uTexture, vTex + vec2( o.x, -o.y))
           + texture2D(uTexture, vTex + vec2(-o.x,  o.y))
           + texture2D(uTexture, vTex + vec2( o.x,  o.y));
    gl_FragColor = c * 0.25;
}

)KF"
    },
    { "glow_bloom",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
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

)KF"
    },
    { "gradient_4color",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;
        uniform vec4 uLayerRect;
        uniform float uP1x;
        uniform float uP1y;
        uniform float uP2x;
        uniform float uP2y;
        uniform float uP3x;
        uniform float uP3y;
        uniform float uP4x;
        uniform float uP4y;
        uniform float uC1R;
        uniform float uC1G;
        uniform float uC1B;
        uniform float uC2R;
        uniform float uC2G;
        uniform float uC2B;
        uniform float uC3R;
        uniform float uC3G;
        uniform float uC3B;
        uniform float uC4R;
        uniform float uC4G;
        uniform float uC4B;
        uniform float uBlend;
        uniform float uFitAlpha;
        uniform float uBlendOriginal;
        varying vec2 vTex;


float insideLayer(vec2 uv) {
    vec2 lo = step(uLayerRect.xy, uv);
    vec2 hi = step(uv, uLayerRect.zw);
    return lo.x * lo.y * hi.x * hi.y;
}


float fitCoverage(float baseAlpha) {
    return mix(1.0, baseAlpha, step(0.5, uFitAlpha));
}


float fitInside(vec2 uv) {
    return mix(insideLayer(uv), 1.0, step(0.5, uFitAlpha));
}


        void main() {
            vec4 base = texture2D(uTexture, vTex);
            float amount = fitInside(vTex) * (1.0 - clamp(uBlendOriginal, 0.0, 1.0));
            float cover = fitCoverage(base.a);
            if (amount <= 0.0 || cover <= 0.0) {
                gl_FragColor = base;
                return;
            }

            float aspect = uTexelSize.y / uTexelSize.x;
            vec2 sq = vec2(aspect, 1.0);
            vec2 size = max(uLayerRect.zw - uLayerRect.xy, vec2(1e-4));
            vec2 p = (vTex - uLayerRect.xy) / size;   // 0..1 across the layer

            // The floor on the distance is what keeps a point from becoming a
            // singularity: without it the pixel exactly under a point divides by
            // zero and renders as a bright speck.
            vec4 w = vec4(
                1.0 / pow(max(length((p - vec2(uP1x, uP1y)) * sq), 1e-3), uBlend),
                1.0 / pow(max(length((p - vec2(uP2x, uP2y)) * sq), 1e-3), uBlend),
                1.0 / pow(max(length((p - vec2(uP3x, uP3y)) * sq), 1e-3), uBlend),
                1.0 / pow(max(length((p - vec2(uP4x, uP4y)) * sq), 1e-3), uBlend)
            );
            // Normalised, so the four weights always sum to one and the space
            // between the points stays as bright as the points themselves.
            w /= max(w.x + w.y + w.z + w.w, 1e-4);

            vec3 col = vec3(uC1R, uC1G, uC1B) * w.x
                     + vec3(uC2R, uC2G, uC2B) * w.y
                     + vec3(uC3R, uC3G, uC3B) * w.z
                     + vec3(uC4R, uC4G, uC4B) * w.w;
            gl_FragColor = mix(base, vec4(clamp(col, 0.0, 1.0) * cover, cover), amount);
        }

)KF"
    },
    { "gradient_linear_radial",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;
        uniform vec4 uLayerRect;
        uniform float uStartX;
        uniform float uStartY;
        uniform float uEndX;
        uniform float uEndY;
        uniform float uStartR;
        uniform float uStartG;
        uniform float uStartB;
        uniform float uEndR;
        uniform float uEndG;
        uniform float uEndB;
        uniform float uRadial;
        uniform float uFitAlpha;
        uniform float uBlendOriginal;
        varying vec2 vTex;


float insideLayer(vec2 uv) {
    vec2 lo = step(uLayerRect.xy, uv);
    vec2 hi = step(uv, uLayerRect.zw);
    return lo.x * lo.y * hi.x * hi.y;
}


float fitCoverage(float baseAlpha) {
    return mix(1.0, baseAlpha, step(0.5, uFitAlpha));
}


float fitInside(vec2 uv) {
    return mix(insideLayer(uv), 1.0, step(0.5, uFitAlpha));
}


        void main() {
            vec4 base = texture2D(uTexture, vTex);
            float amount = fitInside(vTex) * (1.0 - clamp(uBlendOriginal, 0.0, 1.0));
            float cover = fitCoverage(base.a);
            if (amount <= 0.0 || cover <= 0.0) {
                gl_FragColor = base;
                return;
            }

            float aspect = uTexelSize.y / uTexelSize.x;
            vec2 size = max(uLayerRect.zw - uLayerRect.xy, vec2(1e-4));
            vec2 s = uLayerRect.xy + vec2(uStartX, uStartY) * size;
            vec2 e = uLayerRect.xy + vec2(uEndX, uEndY) * size;
            // Height units for both ends, so a radial ramp is a circle and a linear
            // one runs at the angle it looks like it runs at.
            vec2 sq = vec2(aspect, 1.0);
            vec2 d = (e - s) * sq;
            vec2 v = (vTex - s) * sq;

            float t = (uRadial > 0.5)
                ? length(v) / max(length(d), 1e-4)
                : dot(v, d) / max(dot(d, d), 1e-4);
            t = clamp(t, 0.0, 1.0);

            vec3 col = mix(vec3(uStartR, uStartG, uStartB), vec3(uEndR, uEndG, uEndB), t);
            gl_FragColor = mix(base, vec4(col * cover, cover), amount);
        }

)KF"
    },
    { "grain",
R"KF(precision mediump float;
uniform sampler2D uTexture;
uniform vec4 uLayerRect;
uniform float uAmount;
uniform float uSize;
uniform float uSoftness;
varying vec2 vTex;
void main() {
    vec4 c = texture2D(uTexture, vTex);
    vec2 centre = (uLayerRect.xy + uLayerRect.zw) * 0.5;
    // Never zero: a degenerate layer rect would divide the whole frame to
    // infinity and darken every pixel of it.
    vec2 halfBox = max((uLayerRect.zw - uLayerRect.xy) * 0.5, vec2(1e-4));
    // -1..1 across the layer, so d = 1 at the edges and 1.41 in the corners.
    float d = length((vTex - centre) / halfBox);
    float v = smoothstep(uSize, uSize + uSoftness, d);
    // RGB only. Premultiplied, so scaling colour without alpha is a darken.
    gl_FragColor = vec4(c.rgb * (1.0 - clamp(uAmount, 0.0, 1.0) * v), c.a);
}

)KF"
    },
    { "halftone_cmyk",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;   // 1/w, 1/h of the target
        uniform float uCellSize;
        uniform float uAngle;
        uniform float uCmyk;
        uniform float uGain;
        uniform float uSoftness;
        uniform float uInkR;
        uniform float uInkG;
        uniform float uInkB;
        uniform float uPaperEnabled;
        uniform float uPaperR;
        uniform float uPaperG;
        uniform float uPaperB;
        varying vec2 vTex;

        vec2 turn(vec2 v, float a) {
            float c = cos(a);
            float s = sin(a);
            return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
        }

        // One screen: the average (premultiplied) colour of the cell this fragment
        // falls in on a grid turned by `ang`, and through `dist` how far the fragment
        // sits from that cell's centre, in cell units. Four stratified taps — a point
        // sample of detailed footage picks a different texel every time the picture
        // moves a hair, and the dots would flicker.
        vec4 screenCell(vec2 p, float cell, float ang, vec2 toUnits, out float dist) {
            vec2 r = turn(p, ang) / cell;
            vec2 idx = floor(r);
            dist = length(fract(r) - 0.5);
            vec2 centreUv = turn((idx + 0.5) * cell, -ang) / toUnits + 0.5;
            vec4 sum = vec4(0.0);
            for (int j = 0; j < 2; j++) {
                for (int i = 0; i < 2; i++) {
                    vec2 o = (vec2(float(i), float(j)) - 0.5) * 0.5;   // ±0.25 of the cell
                    sum += texture2D(uTexture, centreUv + turn(o * cell, -ang) / toUnits);
                }
            }
            return sum * 0.25;
        }

        vec3 straight(vec4 c) {
            return clamp(c.rgb / max(c.a, 1e-4), 0.0, 1.0);
        }

        // RGB → CMYK with under-colour removal: the grey the three inks share
        // moves onto the black plate.
        vec4 toCmyk(vec3 s) {
            vec3 cmy = 1.0 - s;
            float k = min(cmy.r, min(cmy.g, cmy.b));
            return vec4((cmy - k) / max(1.0 - k, 1e-4), k);
        }

        // How much of this fragment a dot of the given tone covers.
        float dotCover(float darkness, float dist, float aa) {
            // Dot Gain as a gamma on the tone. pow(0, y) is undefined in GLSL ES,
            // and its NaN survives every guard after it — floor the base.
            float k = pow(max(clamp(darkness, 0.0, 1.0), 1e-4), 1.0 / max(uGain, 1e-2));
            // Area ∝ tone: r = sqrt(k / π) covers exactly k of the cell — up to the
            // deep shadows, then a shoulder past the half-diagonal (0.707) so black
            // prints solid instead of leaving a paper speck in every corner.
            float r = 0.5642 * sqrt(k) + smoothstep(0.85, 1.0, k) * (0.78 - 0.5642);
            // The band sits INSIDE the radius — centred on it, a zero-radius dot
            // would still half-light its centre texel and white would be freckled.
            return 1.0 - smoothstep(r - 2.0 * aa, r, dist);
        }

        void main() {
            vec4 src = texture2D(uTexture, vTex);
            float a = src.a;


            float aspect = uTexelSize.y / uTexelSize.x;   // width / height
            vec2 toUnits = vec2(aspect, 1.0);
            vec2 p = (vTex - 0.5) * toUnits;
            float cell = max(uCellSize * (1.0 / 1080.0), uTexelSize.y);

            float base = radians(uAngle);
            // Half a texel each side in cell units, widened by Softness toward a
            // blurred tone.
            float aa = 0.5 * uTexelSize.y / cell + clamp(uSoftness, 0.0, 1.0) * 0.3;

            vec3 trans;   // what the ink lets through, per channel: 1 = bare paper
            float cov;    // how much of this fragment is inked at all
            if (uCmyk > 0.5) {
                // The press angles — C 15°, M 75°, Y 0°, K 45° — relative to Angle,
                // which is where the black screen sits. Each ink reads its OWN cell.
                float dC; float dM; float dY; float dK;
                vec3 sC = straight(screenCell(p, cell, base - radians(30.0), toUnits, dC));
                vec3 sM = straight(screenCell(p, cell, base + radians(30.0), toUnits, dM));
                vec3 sY = straight(screenCell(p, cell, base - radians(45.0), toUnits, dY));
                vec3 sK = straight(screenCell(p, cell, base, toUnits, dK));
                float c = dotCover(toCmyk(sC).x, dC, aa);
                float m = dotCover(toCmyk(sM).y, dM, aa);
                float y = dotCover(toCmyk(sY).z, dY, aa);
                float k = dotCover(toCmyk(sK).w, dK, aa);
                // Process inks as filters: cyan takes the red out, magenta the green,
                // yellow the blue, black everything. The product is the overprint.
                trans = (1.0 - c * vec3(1.0, 0.0, 0.0))
                      * (1.0 - m * vec3(0.0, 1.0, 0.0))
                      * (1.0 - y * vec3(0.0, 0.0, 1.0))
                      * (1.0 - k);
                cov = 1.0 - (1.0 - c) * (1.0 - m) * (1.0 - y) * (1.0 - k);
            } else {
                float d;
                vec3 s = straight(screenCell(p, cell, base, toUnits, d));
                float dark = 1.0 - dot(s, vec3(0.2126, 0.7152, 0.0722));
                cov = dotCover(dark, d, aa);
                trans = mix(vec3(1.0), vec3(uInkR, uInkG, uInkB), cov);
            }

            // The ink's own colour, straight: `trans` with the bare paper taken back
            // out of it (over white, trans = (1 - cov) + inkColour * cov).
            vec3 inkColour = cov > 1e-4 ? clamp((trans - (1.0 - cov)) / cov, 0.0, 1.0) : vec3(0.0);
            if (uPaperEnabled > 0.5) {
                // Printed ON the paper: paper where there is no ink, ink where there
                // is. Gated by the layer's own alpha, so the print lands inside the
                // artwork's silhouette and never across its box.
                vec3 rgb = mix(vec3(uPaperR, uPaperG, uPaperB), inkColour, cov);
                gl_FragColor = vec4(rgb * a, a);
            } else {
                // Just the dots, over whatever is underneath.
                gl_FragColor = vec4(inkColour * cov, cov) * a;
            }
        }

)KF"
    },
    { "highlight_recovery",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
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

)KF"
    },
    { "highlight_suppress",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
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

)KF"
    },
    { "hsl_adjust",
R"KF(precision mediump float;
uniform sampler2D uTexture;
uniform float uHue;
uniform float uSaturation;
uniform float uLightness;
varying vec2 vTex;
void main() {
    vec4 c = texture2D(uTexture, vTex);
    // The layer arrives premultiplied; colour math needs straight RGB, or a
    // semi-transparent edge rotates toward black instead of its own hue.
    float a = c.a;
    vec3 rgb = clamp(c.rgb / max(a, 1e-4), 0.0, 1.0);

    float ang = uHue * 3.14159265;
    float cs = cos(ang);
    float sn = sin(ang);
    // The standard luma-preserving hue rotation, written as three dots
    // rather than a mat3 so the row/column order cannot be read wrongly.
    vec3 hs = vec3(
        dot(rgb, vec3(0.299 + 0.701 * cs + 0.168 * sn,
                      0.587 - 0.587 * cs + 0.330 * sn,
                      0.114 - 0.114 * cs - 0.497 * sn)),
        dot(rgb, vec3(0.299 - 0.299 * cs - 0.328 * sn,
                      0.587 + 0.413 * cs + 0.035 * sn,
                      0.114 - 0.114 * cs + 0.292 * sn)),
        dot(rgb, vec3(0.299 - 0.300 * cs + 1.250 * sn,
                      0.587 - 0.588 * cs - 1.050 * sn,
                      0.114 + 0.886 * cs - 0.203 * sn))
    );

    float luma = dot(hs, vec3(0.2126, 0.7152, 0.0722));
    hs = mix(vec3(luma), hs, 1.0 + uSaturation);
    // Lightness lifts toward white or crushes toward black, as AE's does —
    // NOT a multiply, which would only ever darken.
    hs = (uLightness >= 0.0)
        ? mix(hs, vec3(1.0), uLightness)
        : mix(hs, vec3(0.0), -uLightness);

    hs = clamp(hs, 0.0, 1.0);
    gl_FragColor = vec4(hs * a, a);
}

)KF"
    },
    { "hue_saturation",
R"KF(        precision mediump float;
        uniform sampler2D uTexture;
        uniform float uHue;
        uniform float uLightness;
        uniform float uSaturation;
        varying vec2 vTex;

        vec3 toHsl(vec3 c) {
            float mx = max(c.r, max(c.g, c.b));
            float mn = min(c.r, min(c.g, c.b));
            float l = (mx + mn) * 0.5;
            float d = mx - mn;
            float h = 0.0;
            float s = 0.0;
            if (d > 1e-5) {
                s = (l > 0.5) ? d / max(2.0 - mx - mn, 1e-5) : d / max(mx + mn, 1e-5);
                if (mx == c.r) {
                    h = (c.g - c.b) / d + ((c.g < c.b) ? 6.0 : 0.0);
                } else if (mx == c.g) {
                    h = (c.b - c.r) / d + 2.0;
                } else {
                    h = (c.r - c.g) / d + 4.0;
                }
                h /= 6.0;
            }
            return vec3(h, s, l);
        }

        float channel(float p, float q, float t) {
            t = fract(t);
            if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
            if (t < 0.5) return q;
            if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
            return p;
        }

        vec3 toRgb(vec3 hsl) {
            // Grey has no hue to reconstruct; the general path would divide by a
            // saturation of zero and speckle every neutral pixel.
            if (hsl.y < 1e-5) return vec3(hsl.z);
            float q = (hsl.z < 0.5)
                ? hsl.z * (1.0 + hsl.y)
                : hsl.z + hsl.y - hsl.z * hsl.y;
            float p = 2.0 * hsl.z - q;
            return vec3(
                channel(p, q, hsl.x + 1.0 / 3.0),
                channel(p, q, hsl.x),
                channel(p, q, hsl.x - 1.0 / 3.0)
            );
        }

        void main() {

vec4 src = texture2D(uTexture, vTex);
float a = src.a;
vec3 rgb = clamp(src.rgb / max(a, 1e-4), 0.0, 1.0);

            vec3 hsl = toHsl(rgb);
            // Hue wraps: -1..1 is a full turn either way, and fract() in `channel`
            // takes care of the wrap-around at red.
            hsl.x = hsl.x + uHue * 0.5;
            hsl.z = (uLightness >= 0.0)
                ? mix(hsl.z, 1.0, uLightness)
                : mix(hsl.z, 0.0, -uLightness);
            hsl.y = clamp(hsl.y * (1.0 + uSaturation), 0.0, 1.0);
            vec3 v = clamp(toRgb(hsl), 0.0, 1.0);
            gl_FragColor = vec4(v * a, a);
        }

)KF"
    },
    { "invert",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;   // 1/w, 1/h of the target
        uniform float uInvert;
        uniform float uWidth;
        uniform float uIntensity;
        uniform float uBlend;
        varying vec2 vTex;

        void main() {
            vec4 src = texture2D(uTexture, vTex);
            float a = src.a;

            float aspect = uTexelSize.y / uTexelSize.x;   // width / height
            // Frame-height units, x divided by the aspect so the kernel stays square
            // on a 16:9 comp — a horizontal edge and a vertical one of the same
            // contrast must come back the same weight.
            vec2 span = vec2(1.0 / aspect, 1.0) * (uWidth * (1.0 / 1080.0));
            // Never below one texel of THIS target. In a half-resolution preview the
            // frame-height distance lands inside a texel, and a sub-texel Sobel reads
            // the same bilinear sample twice and returns zero — the effect would
            // simply vanish while scrubbing and reappear in the export.
            vec2 d = max(span, uTexelSize);

            // Premultiplied, deliberately — see the doc comment. Eight taps; Sobel
            // gives the centre weight 0, so it is never fetched for the kernel.
            vec3 c00 = texture2D(uTexture, vTex + vec2(-d.x, -d.y)).rgb;
            vec3 c10 = texture2D(uTexture, vTex + vec2( 0.0, -d.y)).rgb;
            vec3 c20 = texture2D(uTexture, vTex + vec2( d.x, -d.y)).rgb;
            vec3 c01 = texture2D(uTexture, vTex + vec2(-d.x,  0.0)).rgb;
            vec3 c21 = texture2D(uTexture, vTex + vec2( d.x,  0.0)).rgb;
            vec3 c02 = texture2D(uTexture, vTex + vec2(-d.x,  d.y)).rgb;
            vec3 c12 = texture2D(uTexture, vTex + vec2( 0.0,  d.y)).rgb;
            vec3 c22 = texture2D(uTexture, vTex + vec2( d.x,  d.y)).rgb;

            // Sobel, per channel rather than on a luminance: that is what makes the
            // edges carry the colour of the transition.
            vec3 gx = (c20 + 2.0 * c21 + c22) - (c00 + 2.0 * c01 + c02);
            vec3 gy = (c02 + 2.0 * c12 + c22) - (c00 + 2.0 * c10 + c20);
            vec3 g = clamp(sqrt(gx * gx + gy * gy) * uIntensity, 0.0, 1.0);

            // AE: default is dark lines on white, Invert is bright lines on black.
            vec3 edge = mix(1.0 - g, g, step(0.5, uInvert));

            vec3 straight = src.rgb / max(a, 1e-4);
            vec3 outRgb = clamp(mix(edge, straight, uBlend), 0.0, 1.0);
            // Coverage is untouched, and the ground is multiplied by it: the drawing
            // lands inside the artwork's own silhouette, never across its box.
            gl_FragColor = vec4(outRgb * a, a);
        }

)KF"
    },
    { "levels",
R"KF(        precision mediump float;
        uniform sampler2D uTexture;
        uniform float uInBlack;
        uniform float uInWhite;
        uniform float uGamma;
        uniform float uOutBlack;
        uniform float uOutWhite;
        varying vec2 vTex;

        float remap(float c) {
            // The guard is not paranoia: dragging input white below input black is a
            // normal thing to do by accident, and an unguarded divide turns the whole
            // layer into NaN, which renders as black or as nothing depending on the
            // driver.
            float t = clamp((c - uInBlack) / max(uInWhite - uInBlack, 1e-4), 0.0, 1.0);
            t = pow(t, 1.0 / max(uGamma, 1e-3));
            return uOutBlack + t * (uOutWhite - uOutBlack);
        }

        void main() {

vec4 src = texture2D(uTexture, vTex);
float a = src.a;
vec3 rgb = clamp(src.rgb / max(a, 1e-4), 0.0, 1.0);

            vec3 v = clamp(vec3(remap(rgb.r), remap(rgb.g), remap(rgb.b)), 0.0, 1.0);
            gl_FragColor = vec4(v * a, a);
        }

)KF"
    },
    { "light_flare",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;   // pass-1 output: blurred alpha in .a
        uniform sampler2D uInput;     // the layer as this effect received it
        uniform vec2 uTexelSize;
        uniform vec4 uLayerRect;
        uniform float uCenterX;
        uniform float uCenterY;
        uniform float uDirection;
        uniform float uShape;
        uniform float uWidth;
        uniform float uSweepIntensity;
        uniform float uEdgeIntensity;
        uniform float uEdgeThickness;
        uniform float uColorR;
        uniform float uColorG;
        uniform float uColorB;
        uniform float uLightReception;
        varying vec2 vTex;
        const int K = 8;

        void main() {
            vec4 base = texture2D(uInput, vTex);

            float step = max(uEdgeThickness, 1e-4) / float(K);
            float sum = 0.0;
            float wsum = 0.0;
            for (int i = -K; i <= K; i++) {
                float w = exp(-3.0 * float(i * i) / float(K * K));
                sum += texture2D(uTexture, vTex + vec2(0.0, float(i) * step)).a * w;
                wsum += w;
            }
            float soft = sum / wsum;
            // Rim: covered here, but the coverage blurred over Edge Thickness is not —
            // so the alpha edge is within that distance. A pixel right on the edge
            // blurs to one half, hence the doubling: the edge itself is a full rim.
            float rim = clamp(base.a * (1.0 - soft) * 2.0, 0.0, 1.0);

            float aspect = uTexelSize.y / uTexelSize.x;
            vec2 size = max(uLayerRect.zw - uLayerRect.xy, vec2(1e-4));
            vec2 centre = uLayerRect.xy + vec2(uCenterX, uCenterY) * size;
            // AE's angle: 0 stands the band upright and it turns clockwise on
            // screen. uv's y points up, so the band's normal at angle a is
            // (cos a, -sin a). Distance in frame-height units, so Width means the
            // same thickness at any angle.
            float ang = radians(uDirection);
            vec2 n = vec2(cos(ang), -sin(ang));
            float x = abs(dot((vTex - centre) * vec2(aspect, 1.0), n));
            float w = max(uWidth, 1e-4);
            float t = clamp(1.0 - x / w, 0.0, 1.0);   // 1 on the centre line, 0 at the edge

            int shape = int(uShape + 0.5);
            float body;
            if (shape == 0) {
                body = t;                                 // Linear
            } else if (shape == 1) {
                body = 0.5 - 0.5 * cos(t * 3.14159265);   // Smooth
            } else {
                body = t * t * t;                         // Sharp: gathered at the centre
            }

            vec3 light = vec3(uColorR, uColorG, uColorB);
            // The rim only flares where the band is: the light catches the edge as
            // it passes, it does not outline the whole layer.
            float amount = body * (uSweepIntensity + rim * uEdgeIntensity);
            int reception = int(uLightReception + 0.5);
            if (reception == 2) {
                // Cutout: the sweep alone, cut to the layer's shape.
                float cov = clamp(amount, 0.0, 1.0) * base.a;
                gl_FragColor = vec4(light * cov, cov);
            } else if (reception == 1) {
                // Composite: the light laid over the pixel, so a strong sweep goes
                // to the light's colour rather than to white.
                float k = clamp(amount, 0.0, 1.0);
                gl_FragColor = vec4(mix(base.rgb, light * base.a, k), base.a);
            } else {
                // Add. Times the layer's own coverage: the light belongs to the
                // artwork, not to the empty space around it. Alpha is untouched, so
                // the result has to be clamped to it to stay a valid premultiplied
                // colour.
                vec3 lit = base.rgb + light * (amount * base.a);
                gl_FragColor = vec4(min(lit, vec3(base.a)), base.a);
            }
        }

)KF"
    },
    { "lut_apply",
R"KF(        precision mediump float;
        uniform sampler2D uTexture;
        // 256 x 1: texel i is what level i/255 becomes, per channel, Master folded in.
        uniform sampler2D uLut;
        varying vec2 vTex;

        void main() {

vec4 src = texture2D(uTexture, vTex);
float a = src.a;
vec3 rgb = clamp(src.rgb / max(a, 1e-4), 0.0, 1.0);

            // Level 0 has to land on the CENTRE of texel 0 and level 1 on the centre
            // of texel 255, or the linear filter would blend the ends with the clamp
            // and the table's black and white would both be off by half a step.
            vec3 t = rgb * (255.0 / 256.0) + 0.5 / 256.0;
            vec3 v = vec3(
                texture2D(uLut, vec2(t.r, 0.5)).r,
                texture2D(uLut, vec2(t.g, 0.5)).g,
                texture2D(uLut, vec2(t.b, 0.5)).b
            );
            gl_FragColor = vec4(v * a, a);
        }

)KF"
    },
    { "mask_apply",
R"KF(precision mediump float;
uniform sampler2D uTexture;     // the mask layer, rendered alone (premultiplied)
uniform sampler2D uBackdrop;    // what is already composited below it
uniform vec2 uTargetSize;
uniform int uMaskMode;
varying vec2 vTex;
void main() {
    vec4 dst = texture2D(uBackdrop, gl_FragCoord.xy / uTargetSize);
    vec4 m = texture2D(uTexture, vTex);
    float k = m.a;
    if (uMaskMode == 3 || uMaskMode == 4) {
        vec3 straight = m.rgb / max(m.a, 1e-4);
        k = dot(straight, vec3(0.299, 0.587, 0.114)) * m.a;
    }
    if (uMaskMode == 2 || uMaskMode == 4) k = 1.0 - k;
    gl_FragColor = dst * clamp(k, 0.0, 1.0);
}

)KF"
    },
    { "matte_choke",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D uTexture;
uniform vec2 uTexelSize;   // 1/w, 1/h of the target
uniform vec4 uLayerRect;   // the layer's box in target uv
uniform float uChoke;
varying vec2 vTex;

void main() {
    vec4 c = texture2D(uTexture, vTex);
    // Choke in 1080p pixels, as texels of THIS target — floored at one texel, so a
    // choke the preview cannot resolve still erodes something rather than nothing.
    float r = max(uChoke * (1.0 / 1080.0) / uTexelSize.y, 1.0);
    if (uChoke > 0.0 && c.a > 0.0) {
        float a = c.a;
        float stride = max(r / 8.0, 1.0);
        for (int i = 1; i <= 8; i++) {
            float o = float(i) * stride;
            if (o > r + 0.001) break;
            vec2 d = o * uTexelSize * vec2(cos(float(i) * 0.7853982), sin(float(i) * 0.7853982));
            vec2 pUv = clamp(vTex + d, uLayerRect.xy, uLayerRect.zw);
            vec2 nUv = clamp(vTex - d, uLayerRect.xy, uLayerRect.zw);
            a = min(a, min(texture2D(uTexture, pUv).a, texture2D(uTexture, nUv).a));
        }
        c *= a / c.a;
    }
    gl_FragColor = c;
}

)KF"
    },
    { "motion_blur_directional",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
uniform sampler2D uTexture;
uniform mat3 uMbStart;
uniform mat3 uMbEnd;
uniform vec2 uTargetSize;
uniform float uMaxVelPx;
uniform int uSamples;
varying vec2 vTex;
void main() {
    vec3 p = vec3(vTex, 1.0);
    vec3 s = uMbStart * p;
    vec3 e = uMbEnd * p;
    vec2 vel = vec2(0.0);
    // Both matrices are normalised to w = 1 at the middle of the layer, so a w this
    // small is a point on the mapping's horizon (or a dead all-zero matrix): there is
    // no honest velocity for it, and a sharp pixel beats a wild streak.
    if (abs(s.z) > 1e-4 && abs(e.z) > 1e-4) vel = e.xy / e.z - s.xy / s.z;
    float travel = length(vel * uTargetSize);
    if (travel > uMaxVelPx) vel *= uMaxVelPx / travel;
    // Interleaved-gradient noise in [0,1): the inner fract keeps every intermediate
    // small, so this stays well-conditioned even in mediump.
    float d = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    vec4 acc = vec4(0.0);
    float n = float(uSamples);
    for (int i = 0; i < 48; i++) {
        if (i >= uSamples) break;
        float t = (float(i) + d) / n - 0.5;
        acc += texture2D(uTexture, vTex + vel * t);
    }
    gl_FragColor = acc / n;
}

)KF"
    },
    { "motion_ripple",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;    // 1/w, 1/h
        uniform vec4 uLayerRect;    // the layer's box, in 0..1 target space
        uniform float uTime;        // seconds since the layer's in point
        uniform float uFrequency;   // jolts per second
        uniform float uStrength;    // travel, fraction of frame height
        uniform float uRotation;    // degrees, peak
        uniform float uScale;       // peak zoom, as a fraction (0.02 = +/-2%)
        uniform float uSoften;      // 0 = held steps, 1 = eased between them
        uniform float uDecay;       // 1/seconds; 0 = never settles
        uniform float uSeed;
        varying vec2 vTex;


float falloff(float t, float decay) {
    return exp(-decay * t);
}


vec2 unrotate(vec2 p, float deg, float aspect) {
    p.x *= aspect;
    float a = -radians(deg);
    float c = cos(a);
    float s = sin(a);
    p = mat2(c, -s, s, c) * p;
    p.x /= aspect;
    return p;
}


vec4 sampleOrNothing(sampler2D tex, vec2 uv) {
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return vec4(0.0);
    return texture2D(tex, uv);
}


uniform sampler2D uWideSource;
uniform float uWideCopy;
uniform vec4 uWideFit;        // (kx, ky, ox, oy): layer (s, t) -> copy (s*kx+ox, t*ky+oy)
uniform mat3 uLayerToLocal;   // target uv -> (s*w, t*w, w) in the LAYER's space
uniform mat3 uPreMotion;      // layer point -> the layer point drawn there by the passes above
uniform float uPreMotionOn;
uniform float uFoldFade;      // 1 - opacity of the Transforms folded into uPreMotion

vec4 sampleLayer(vec2 uv) {
    if (uWideCopy < 0.5) return sampleOrNothing(uTexture, uv);
    vec3 q = uLayerToLocal * vec3(uv, 1.0);
    // At or past the horizon of a tilted plane there is no layer point (a degenerate
    // matrix never reaches here: the renderer draws no copy for a layer it cannot lay
    // flat).
    if (q.z < 1e-4) return vec4(0.0);
    vec2 p = q.xy / q.z;
    if (uPreMotionOn > 0.5) {
        vec3 m = uPreMotion * vec3(p, 1.0);
        if (m.z <= 0.0) return vec4(0.0);
        p = m.xy / m.z;
    }
    vec4 fit = uWideFit.x > 0.0 ? uWideFit : vec4(1.0, 1.0, 0.0, 0.0);
    // t runs DOWN the layer, the texture's v runs up.
    vec2 c = vec2(p.x * fit.x + fit.z, 1.0 - (p.y * fit.y + fit.w));
    if (c.x < 0.0 || c.x > 1.0 || c.y < 0.0 || c.y > 1.0) return vec4(0.0);
    return texture2D(uWideSource, c) * (1.0 - uFoldFade);
}


        // Hash without sine: sin-based hashes differ between GPU vendors, and a
        // shake that lands on different values on the export device than in the
        // preview is not the same shot.
        float hash11(float p) {
            p = fract(p * 0.1031);
            p *= p + 33.33;
            p *= p + p;
            return fract(p);
        }

        // One value per step of one random stream, in -1..1. Held flat across the
        // step; uSoften eases it into the next one instead.
        float jolt(float stream, float tick, float f) {
            float a = hash11(tick + stream);
            float b = hash11(tick + 1.0 + stream);
            float k = smoothstep(0.0, 1.0, f) * clamp(uSoften, 0.0, 1.0);
            return mix(a, b, k) * 2.0 - 1.0;
        }

        void main() {
            float aspect = uTexelSize.y / uTexelSize.x;
            float t = max(uTime, 0.0);
            float ticks = t * uFrequency;
            float tick = floor(ticks);
            float f = ticks - tick;
            // Streams far enough apart that no two ever walk the same sequence.
            float seed = uSeed * 37.0;
            float fall = falloff(t, uDecay);

            vec2 off = vec2(
                jolt(seed, tick, f) / aspect,
                jolt(seed + 11.0, tick, f)
            ) * (uStrength * fall);
            float rot = jolt(seed + 23.0, tick, f) * uRotation * fall;
            float scale = 1.0 + jolt(seed + 41.0, tick, f) * uScale * fall;

            // Turns and zooms about the layer's own centre — a shake pivoting on the
            // frame centre would fling an off-centre layer across the screen.
            vec2 pivot = (uLayerRect.xy + uLayerRect.zw) * 0.5;
            vec2 p = vTex - pivot - off;
            p = unrotate(p, rot, aspect);
            p /= max(scale, 1e-4);
            gl_FragColor = sampleLayer(pivot + p);
        }

)KF"
    },
    { "motion_swirl",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;    // 1/w, 1/h
        uniform vec4 uLayerRect;    // the layer's box, in 0..1 target space
        uniform float uTime;        // seconds since the layer's in point
        uniform float uSpeed;       // swings per second
        uniform float uAngle;       // degrees to each side
        uniform float uPivotX;      // 0..1 across the layer
        uniform float uPivotY;      // 0..1 DOWN the layer (0 = its top edge)
        uniform float uPhase;       // degrees: where in the swing t = 0 sits
        uniform float uDecay;       // 1/seconds; 0 = never settles
        varying vec2 vTex;


float turns(float t, float speed, float phaseDeg) {
    return fract(t * speed + phaseDeg / 360.0);
}


float falloff(float t, float decay) {
    return exp(-decay * t);
}


vec2 unrotate(vec2 p, float deg, float aspect) {
    p.x *= aspect;
    float a = -radians(deg);
    float c = cos(a);
    float s = sin(a);
    p = mat2(c, -s, s, c) * p;
    p.x /= aspect;
    return p;
}


vec4 sampleOrNothing(sampler2D tex, vec2 uv) {
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return vec4(0.0);
    return texture2D(tex, uv);
}


uniform sampler2D uWideSource;
uniform float uWideCopy;
uniform vec4 uWideFit;        // (kx, ky, ox, oy): layer (s, t) -> copy (s*kx+ox, t*ky+oy)
uniform mat3 uLayerToLocal;   // target uv -> (s*w, t*w, w) in the LAYER's space
uniform mat3 uPreMotion;      // layer point -> the layer point drawn there by the passes above
uniform float uPreMotionOn;
uniform float uFoldFade;      // 1 - opacity of the Transforms folded into uPreMotion

vec4 sampleLayer(vec2 uv) {
    if (uWideCopy < 0.5) return sampleOrNothing(uTexture, uv);
    vec3 q = uLayerToLocal * vec3(uv, 1.0);
    // At or past the horizon of a tilted plane there is no layer point (a degenerate
    // matrix never reaches here: the renderer draws no copy for a layer it cannot lay
    // flat).
    if (q.z < 1e-4) return vec4(0.0);
    vec2 p = q.xy / q.z;
    if (uPreMotionOn > 0.5) {
        vec3 m = uPreMotion * vec3(p, 1.0);
        if (m.z <= 0.0) return vec4(0.0);
        p = m.xy / m.z;
    }
    vec4 fit = uWideFit.x > 0.0 ? uWideFit : vec4(1.0, 1.0, 0.0, 0.0);
    // t runs DOWN the layer, the texture's v runs up.
    vec2 c = vec2(p.x * fit.x + fit.z, 1.0 - (p.y * fit.y + fit.w));
    if (c.x < 0.0 || c.x > 1.0 || c.y < 0.0 || c.y > 1.0) return vec4(0.0);
    return texture2D(uWideSource, c) * (1.0 - uFoldFade);
}


        void main() {
            float aspect = uTexelSize.y / uTexelSize.x;
            float t = max(uTime, 0.0);
            float rot = uAngle * sin(6.2831853 * turns(t, uSpeed, uPhase)) *
                        falloff(t, uDecay);

            vec2 layerMin = uLayerRect.xy;
            vec2 layerSize = max(uLayerRect.zw - uLayerRect.xy, vec2(1e-4));
            // 1 - pivotY: uv y points up, the panel's y points down.
            vec2 pivot = layerMin + vec2(uPivotX, 1.0 - uPivotY) * layerSize;

            vec2 p = unrotate(vTex - pivot, rot, aspect);
            gl_FragColor = sampleLayer(pivot + p);
        }

)KF"
    },
    { "motion_wiggle",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;    // 1/w, 1/h
        uniform float uTime;        // seconds since the layer's in point
        uniform float uSpeed;       // cycles per second
        uniform float uStrength;    // travel, fraction of frame height
        uniform float uAngle;       // degrees: direction of travel
        uniform float uPhase;       // degrees: where in the cycle t = 0 sits
        uniform float uDecay;       // 1/seconds; 0 = never settles
        varying vec2 vTex;


float turns(float t, float speed, float phaseDeg) {
    return fract(t * speed + phaseDeg / 360.0);
}


float falloff(float t, float decay) {
    return exp(-decay * t);
}


vec4 sampleOrNothing(sampler2D tex, vec2 uv) {
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return vec4(0.0);
    return texture2D(tex, uv);
}


uniform sampler2D uWideSource;
uniform float uWideCopy;
uniform vec4 uWideFit;        // (kx, ky, ox, oy): layer (s, t) -> copy (s*kx+ox, t*ky+oy)
uniform mat3 uLayerToLocal;   // target uv -> (s*w, t*w, w) in the LAYER's space
uniform mat3 uPreMotion;      // layer point -> the layer point drawn there by the passes above
uniform float uPreMotionOn;
uniform float uFoldFade;      // 1 - opacity of the Transforms folded into uPreMotion

vec4 sampleLayer(vec2 uv) {
    if (uWideCopy < 0.5) return sampleOrNothing(uTexture, uv);
    vec3 q = uLayerToLocal * vec3(uv, 1.0);
    // At or past the horizon of a tilted plane there is no layer point (a degenerate
    // matrix never reaches here: the renderer draws no copy for a layer it cannot lay
    // flat).
    if (q.z < 1e-4) return vec4(0.0);
    vec2 p = q.xy / q.z;
    if (uPreMotionOn > 0.5) {
        vec3 m = uPreMotion * vec3(p, 1.0);
        if (m.z <= 0.0) return vec4(0.0);
        p = m.xy / m.z;
    }
    vec4 fit = uWideFit.x > 0.0 ? uWideFit : vec4(1.0, 1.0, 0.0, 0.0);
    // t runs DOWN the layer, the texture's v runs up.
    vec2 c = vec2(p.x * fit.x + fit.z, 1.0 - (p.y * fit.y + fit.w));
    if (c.x < 0.0 || c.x > 1.0 || c.y < 0.0 || c.y > 1.0) return vec4(0.0);
    return texture2D(uWideSource, c) * (1.0 - uFoldFade);
}


        void main() {
            float aspect = uTexelSize.y / uTexelSize.x;   // width / height
            // A layer can be sampled before its own in point (motion blur reaches
            // back across the shutter); the clock must not run backwards there.
            float t = max(uTime, 0.0);
            float wave = sin(6.2831853 * turns(t, uSpeed, uPhase));
            float amp = uStrength * falloff(t, uDecay);
            float ang = radians(uAngle);
            // x divided by the aspect: the offset is measured in frame HEIGHTS, so
            // the same Strength travels the same distance whichever way it points.
            vec2 off = vec2(cos(ang) / aspect, sin(ang)) * (wave * amp);
            // MINUS: moving the picture one way means reading from the other.
            gl_FragColor = sampleLayer(vTex - off);
        }

)KF"
    },
    { "noise_fractal",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;
        uniform vec4 uLayerRect;
        uniform float uFractalType;
        uniform float uNoiseType;
        uniform float uInvert;
        uniform float uContrast;
        uniform float uBrightness;
        uniform float uOverflow;
        uniform float uRotation;
        uniform float uScale;
        uniform float uScaleWidth;
        uniform float uScaleHeight;
        uniform float uOffsetX;
        uniform float uOffsetY;
        uniform float uComplexity;
        uniform float uSubInfluence;
        uniform float uSubScaling;
        uniform float uSubRotation;
        uniform float uSubOffsetX;
        uniform float uSubOffsetY;
        uniform float uEvolution;
        uniform float uRandomSeed;
        uniform float uFitAlpha;
        uniform float uOpacity;
        varying vec2 vTex;


float insideLayer(vec2 uv) {
    vec2 lo = step(uLayerRect.xy, uv);
    vec2 hi = step(uv, uLayerRect.zw);
    return lo.x * lo.y * hi.x * hi.y;
}


float fitCoverage(float baseAlpha) {
    return mix(1.0, baseAlpha, step(0.5, uFitAlpha));
}


float fitInside(vec2 uv) {
    return mix(insideLayer(uv), 1.0, step(0.5, uFitAlpha));
}


        // No sin() in the hash: sin-based hashes lose their high frequencies on the
        // mobile GPUs that evaluate them at reduced precision, and the noise then
        // shows a visible grid. This one is pure fract/multiply.
        float hash(vec3 p) {
            p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
            p *= 17.0;
            return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
        }

        // Value noise; noiseType picks the curve between lattice points on x/y.
        // Linear shows the lattice as diamond-shaped creases, which is what AE's
        // Linear looks like too. The z (Evolution) axis is always smoothed: a Block
        // field should boil in place, not pop between cells.
        float valueNoise(vec3 x, int noiseType) {
            vec3 i = floor(x);
            vec3 f = fract(x);
            if (noiseType == 0) {
                f.xy = step(0.5, f.xy);                                  // Block
            } else if (noiseType == 2) {
                f.xy = f.xy * f.xy * (3.0 - 2.0 * f.xy);                 // Soft Linear
            } else if (noiseType == 3) {
                f.xy = f.xy * f.xy * f.xy * (f.xy * (f.xy * 6.0 - 15.0) + 10.0); // Spline
            }
            f.z = f.z * f.z * (3.0 - 2.0 * f.z);
            return mix(
                mix(mix(hash(i + vec3(0.0, 0.0, 0.0)), hash(i + vec3(1.0, 0.0, 0.0)), f.x),
                    mix(hash(i + vec3(0.0, 1.0, 0.0)), hash(i + vec3(1.0, 1.0, 0.0)), f.x), f.y),
                mix(mix(hash(i + vec3(0.0, 0.0, 1.0)), hash(i + vec3(1.0, 0.0, 1.0)), f.x),
                    mix(hash(i + vec3(0.0, 1.0, 1.0)), hash(i + vec3(1.0, 1.0, 1.0)), f.x), f.y),
                f.z
            );
        }

        vec2 rotate2(vec2 v, float degrees) {
            float c = cos(radians(degrees));
            float s = sin(radians(degrees));
            return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
        }

        // Identity through the middle, a rational shoulder over the last quarter at
        // each end that only reaches 0 and 1 at infinity — what Contrast pushed out
        // of range is compressed back rather than flattened into a plateau.
        float softClamp(float x) {
            float k = 0.25;
            float hi = x - (1.0 - k);
            if (hi > 0.0) x = (1.0 - k) + k * hi / (k + hi);
            float lo = k - x;
            if (lo > 0.0) x = k - k * lo / (k + lo);
            return x;
        }

        void main() {
            vec4 base = texture2D(uTexture, vTex);
            float amount = fitInside(vTex) * clamp(uOpacity, 0.0, 1.0);
            float cover = fitCoverage(base.a);
            // Bailing on zero coverage also skips the octave loop for every
            // transparent pixel — most of a text layer's box.
            if (amount <= 0.0 || cover <= 0.0) {
                gl_FragColor = base;
                return;
            }

            int fractalType = int(uFractalType + 0.5);
            int noiseType = int(uNoiseType + 0.5);
            int overflow = int(uOverflow + 0.5);

            float aspect = uTexelSize.y / uTexelSize.x;   // width / height
            // Height units, so a cell stays square and the field is the same size
            // in the reduced-resolution preview as in the export.
            vec2 offset = vec2(uOffsetX, uOffsetY);
            vec2 pos = rotate2(vec2(vTex.x * aspect, vTex.y) - offset, uRotation);
            // Where the frame's centre lands in that space: what Sub Rotation turns
            // the finer octaves about. Turning them about the origin instead would
            // pivot on the frame's top-left corner and sweep the far corner away.
            vec2 pivot = rotate2(vec2(0.5 * aspect, 0.5) - offset, uRotation);
            // Width/height stretch AFTER the rotation, i.e. along the noise's own
            // axes, so a stretched field turns as a whole when Rotation moves.
            vec2 stretch = vec2(max(uScaleWidth, 1e-3), max(uScaleHeight, 1e-3));
            // Cell size of the octave being sampled, in height units.
            float cell = max(uScale, 1e-3);
            // Whole seeds only, so a keyframed seed jumps between fields the way AE's
            // does instead of sliding the lattice through them.
            vec3 seedShift = floor(uRandomSeed + 0.5) * vec3(17.0, 29.0, 7.0);
            float z = uEvolution;

            float sum = 0.0;
            float norm = 0.0;
            float amp = 1.0;
            for (int i = 0; i < 8; i++) {
                // Fractional weight = the octave fades in instead of popping when
                // Complexity is keyframed.
                float w = clamp(uComplexity - float(i), 0.0, 1.0) * amp;
                // Uniform across the draw, so the branch really skips the octaves
                // above Complexity rather than sampling them for a weight of zero.
                if (w > 0.0) {
                    vec3 q = vec3(pos / (cell * stretch), z) + seedShift;
                    float v = valueNoise(q, noiseType);
                    float t = v;
                    if (fractalType == 1) {
                        t = abs(2.0 * v - 1.0);                 // Turbulent Basic
                    } else if (fractalType == 2) {
                        t = sqrt(abs(2.0 * v - 1.0));           // Turbulent Sharp
                    } else if (fractalType == 3) {
                        float a = 2.0 * v - 1.0;
                        t = a * a;                              // Turbulent Smooth
                    }
                    if (fractalType == 5) {
                        sum = max(sum, t * w);                  // Max
)KF"
R"KF(                        norm = 1.0;
                    } else {
                        sum += t * w;
                        norm += w;
                    }
                    if (fractalType == 4) {
                        // Dynamic: this octave's value pushes every finer one
                        // sideways, by up to half a cell each way. The second sample
                        // (a decorrelated slice of the same field) is the y push —
                        // one scalar would only ever smear along a diagonal.
                        float v2 = valueNoise(q + vec3(0.0, 0.0, 37.0), noiseType);
                        pos += (vec2(v, v2) - 0.5) * cell;
                    }
                }
                // Next octave: finer, turned and shifted by the Sub settings.
                amp *= uSubInfluence;
                cell /= max(uSubScaling, 1.0);
                z *= max(uSubScaling, 1.0);
                pos = rotate2(pos - pivot, uSubRotation) + pivot + vec2(uSubOffsetX, uSubOffsetY);
            }
            float n = sum / max(norm, 1e-4);

            // Contrast turns about mid-grey, so it opens the field out symmetrically
            // instead of also darkening it.
            n = (n - 0.5) * uContrast + 0.5 + uBrightness;
            if (overflow == 1) {
                n = softClamp(n);
            } else if (overflow == 2) {
                n = 1.0 - abs(mod(n, 2.0) - 1.0);   // Wrap Back: fold, don't flatten
            }
            n = clamp(n, 0.0, 1.0);
            n = mix(n, 1.0 - n, step(0.5, uInvert));

            // Premultiplied by the coverage it lands with — so on text the noise
            // fills the glyphs and keeps their edges, instead of their box.
            gl_FragColor = mix(base, vec4(vec3(n) * cover, cover), amount);
        }

)KF"
    },
    { "opacity",
R"KF(precision mediump float;
uniform sampler2D uTexture;
uniform float uAlpha;
varying vec2 vTex;
void main() {
    vec4 c = texture2D(uTexture, vTex);
    gl_FragColor = vec4(c.rgb * c.a * uAlpha, c.a * uAlpha);
}

)KF"
    },
    { "pattern_ascii",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform mat3 uLayerToLocal; // target uv -> the LAYER's own space
        uniform mat3 uLocalToUv;    // …and back again
        uniform float uTileCenterX;
        uniform float uTileCenterY;
        uniform float uTileWidth;
        uniform float uTileHeight;
        uniform float uOutputWidth;
        uniform float uOutputHeight;
        uniform float uMirrorEdges;
        uniform float uPhase;
        uniform float uHorizontalPhaseShift;
        // The layer drawn again, FLAT: its own 0..1 space IS this texture's uv (t
        // down the layer = v down the picture, so v is flipped), with ALL of it there
        // whatever the frame cut off. uWideCopy is 1 when it exists; 0 — which is also
        // what an unset uniform reads as — means the layer fitted the frame anyway and
        // the ordinary source is complete. The thumbnail renderer draws no copy and
        // must not be made to look for one. See the note about clipped tiles above.
        uniform sampler2D uWideSource;
        uniform float uWideCopy;
        uniform vec2 uWideTexelSize;  // 1/w, 1/h of the copy
        // Where the layer's own box sits inside the copy — (kx, ky, ox, oy), layer
        // (s, t) at copy (s*kx + ox, t*ky + oy). The copy is padded by whatever the
        // layer draws OUTSIDE its box (a stroke, glyph overhang), for the Motion
        // passes that read it; a tile only ever wants the box. All-zero (unset) means
        // no padding.
        uniform vec4 uWideFit;
        // What the Motion passes ABOVE this one did to the picture, as the map from a
        // layer point to the layer point whose pixel now sits there — in the LAYER's
        // own space. When it is on (uPreMotionOn = 1; 0 is also what unset reads as)
        // the tile reads the layer BEFORE those passes — the flat copy, or uOriginal —
        // through the map, instead of reading their moved pixels out of the scratch:
        //  - the flat copy is the layer's untouched content, so without the map a
        //    Twitch above Motion Tile shook a scratch this path never reads and the
        //    tiles stood still ("tidak kerender ketika di gerakin", any layer bigger
        //    than the frame);
        //  - and the scratch is frame-sized, so a turn (Swing, Twitch's Rotation) had
        //    already lost whatever it swung past the frame edge, and the whole-tile
        //    neighbour search cannot undo a rotation — every tile of a swinging
        //    full-frame photo came back with a black wedge cut out of it.
        // The map is exact for all three, so the search then only has to find
        // periodic neighbours, which it can. Only bound when every pass above this one
        // is a Motion pass (or the copy is in use, where nothing else reaches anyway).
        uniform sampler2D uOriginal;
        uniform mat3 uPreMotion;
        uniform float uPreMotionOn;
        // What the Motion passes BELOW this one would do to the picture, as their
        // sampling map in screen uv (output uv -> the uv they read from). When it is
        // on those passes are not run at all: this pass evaluates its plane at the
        // mapped point instead, which is the same picture with no frame edge in it —
        // the plane is infinite, the scratch those passes slid was not. 0 (unset) =
        // nothing folded, evaluate at vTex.
        uniform mat3 uPostMotion;
        uniform float uPostMotionOn;
        // 1 - the opacity of every Transform folded into this pass (above or below):
        // stated as a fade so that 0 — what unset reads as — means "no fade".
        uniform float uFoldFade;
        uniform vec4 uLayerRect;   // the layer's box in target uv
        uniform vec2 uTexelSize;   // 1/w, 1/h of this target
        // 1 when an earlier pass in this stack MOVES the picture (the Motion family,
        // Transform). See the neighbour search at the bottom — the shader is correct
        // either way, this only says whether it is worth looking.
        uniform float uNeighbourFill;
        varying vec2 vTex;
        // A WARP folded in from below (Wave Warp, Bulge — any pass with a
        // samplingMap) cannot be a 3x3 like uPostMotion, so the renderer compiles a
        // VARIANT of this shader with the warp's own sampling function spliced in
        // here, and its call spliced in at the second marker in main. The plane is
        // then evaluated at the warped point, exactly as for a folded Motion pass,
        // and the warp pass is skipped — run after the tile it read a frame-sized
        // picture and cut the warped edge off. See gl/TileFold.
        // %FOLD_DECLS%

        // One tap, in the LAYER's own 0..1 space: where was that layer point drawn, and
        // what is there? Outside the layer's box this is normally nothing at all —
        // which is exactly what makes the neighbour search below safe.
        vec4 tapLocal(vec2 local) {
            vec2 p = local;
            if (uPreMotionOn > 0.5) {
                // Which layer point did the passes above put here? Asked of the map,
                // because what is read below is the layer BEFORE they ran.
                vec3 m = uPreMotion * vec3(local, 1.0);
                if (m.z <= 0.0) return vec4(0.0);
                p = m.xy / m.z;
            }
            // A layer bigger than the composition left most of itself outside the
            // frame, and the frame is all the ordinary source holds. Read the flat
            // copy instead — for the WHOLE tile, not just the missing parts, so one
            // tile cannot be half sharp and half soft. It holds nothing but the
            // layer's box, so outside it there is nothing, exactly as in the scratch.
            if (uWideCopy > 0.5) {
                if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return vec4(0.0);
                vec4 fit = uWideFit.x > 0.0 ? uWideFit : vec4(1.0, 1.0, 0.0, 0.0);
                return texture2D(uWideSource, vec2(p.x * fit.x + fit.z, 1.0 - (p.y * fit.y + fit.w)));
            }
            vec3 r = uLocalToUv * vec3(p, 1.0);
            if (r.z <= 0.0) return vec4(0.0);
            vec2 uv = r.xy / r.z;
            // The source texture is CLAMP_TO_EDGE; sampling outside it would smear
            // the border pixels across the tile instead of showing nothing.
            if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return vec4(0.0);
            // With the map on, the moved pixels in the scratch are not wanted: the
            // layer as it was drawn is, and the map says where in it to look.
            if (uPreMotionOn > 0.5) return texture2D(uOriginal, uv);
            return texture2D(uTexture, uv);
        }

        void main() {
            // The point this pixel shows: vTex, or — with Motion passes folded in
)KF"
R"KF(            // below — wherever they would have read this pixel from. Everything
            // after this line is the plane evaluated at `uv`, so the whole field
            // moves with them and never runs out at the frame edge.
            vec2 uv = vTex;
            // %FOLD_APPLY%
            if (uPostMotionOn > 0.5) {
                vec3 m = uPostMotion * vec3(vTex, 1.0);
                if (abs(m.z) < 1e-6) {
                    gl_FragColor = vec4(0.0);
                    return;
                }
                uv = m.xy / m.z;
            }

            // Outside the output rectangle nothing is drawn at all — that is how
            // Output W/H shrink the result rather than scaling it. Measured on the
            // FRAME (vTex), never on the mapped point: at the default 100% this
            // rectangle IS the frame, and cropping at the mapped point would cut away
            // exactly the pixels the fold brings in from beyond it — the fold's first
            // build did that, and the field still ran out at the frame edge. The
            // field moves under a fixed window, as it does when the layer is dragged.
            vec2 halfOutput = vec2(uOutputWidth, uOutputHeight) * 0.5;
            vec2 fromCentre = abs(vTex - 0.5);
            if (fromCentre.x > halfOutput.x || fromCentre.y > halfOutput.y) {
                gl_FragColor = vec4(0.0);
                return;
            }

            // Where this screen pixel falls in the LAYER's own 0..1 space. Everything
            // below happens in that space, which is what makes the tiled plane turn
            // and tilt WITH the layer: the grid is nailed to the layer, not to the
            // screen. Tiling in uv instead left an axis-aligned screen grid whose
            // cells each held a separately-rotated copy — under a 3D rotation that
            // read as every duplicate spinning about its own anchor.
            vec3 q = uLayerToLocal * vec3(uv, 1.0);
            if (abs(q.z) < 1e-4) {
                // Two very different things arrive here. A degenerate matrix (a
                // zero-size or edge-on layer) is all zeros, so q is zero too: there is
                // no frame to work in, hand the pixel through untouched. A real
                // vanishing w is the HORIZON of a tilted plane, where q.xy has run off
                // to infinity — past it there is no layer point at all.
                bool inside = uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0;
                gl_FragColor = (dot(q.xy, q.xy) < 1e-8 && inside)
                    ? texture2D(uTexture, uv) : vec4(0.0);
                return;
            }
            if (q.z < 0.0) {          // behind the viewer, on the far side of the horizon
                gl_FragColor = vec4(0.0);
                return;
            }
            vec2 local = q.xy / q.z;

            // A tile is the layer scaled by Tile W/H — so in LAYER space the tile size
            // IS Tile W/H, and Tile Center is already expressed in the same units.
            vec2 tile = max(vec2(uTileWidth, uTileHeight), vec2(1e-4));
            // Position in TILE units: 0 at a tile's centre, ±0.5 at its edges.
            vec2 p = (local - vec2(uTileCenterX, uTileCenterY)) / tile;

            // Phase offsets every other row (or column) along the tiling axis,
            // which is what turns a grid into a brick pattern.
            float phase = uPhase / 360.0;
            if (uHorizontalPhaseShift > 0.5) {
                p.y += phase * floor(p.x + 0.5);
            } else {
                p.x += phase * floor(p.y + 0.5);
            }

            vec2 cell = floor(p + 0.5);
            vec2 f = p - cell;                       // -0.5 .. 0.5 inside the tile
            if (uMirrorEdges > 0.5) {
                // Flip odd cells so neighbouring tiles meet edge-to-edge.
                vec2 odd = mod(abs(cell), 2.0);
                f = mix(f, -f, step(0.5, odd));
            }

            // The WHOLE layer goes into EVERY tile — f spans the layer, not the tile —
            // so Tile W/H at 50% is the layer at half size repeated twice: the video
            // wall Motion Tile exists to make. Sampling a tile-sized window at 1:1
            // instead CROPS: at 50% each tile showed only the middle half of the layer
            // and the rest was never drawn anywhere. It also made Tile W/H above 100%
            // sample outside the layer (transparent) where it should zoom the picture
            // up, and it contradicted this effect's own thumbnail, which promises nine
            // small WHOLE copies.
            // Half a texel INSIDE the layer, never ON its edge. A tile seam samples
            // exactly that edge, and a linear tap there mixes in the transparent margin
            // around the layer — a pale hairline along every seam. Invisible as a trim
            // (it is half a SOURCE texel however small the tile is), fatal as a seam:
            // this is the "putih putih" that appeared along the top and bottom once the
            // wide copy moved the layer's edge off the frame border. In the flat copy
            // the layer fills the texture edge to edge, so the texel is the copy's own.
            // (In a padded copy the layer's box is a fraction of the texture, so
            // half a copy texel is that much MORE in layer units.)
            vec2 fitK = uWideFit.x > 0.0 ? uWideFit.xy : vec2(1.0);
            vec2 inset = uWideCopy > 0.5
                ? 0.5 * uWideTexelSize / fitK
                : 0.5 * uTexelSize / max(uLayerRect.zw - uLayerRect.xy, vec2(1e-4));
            vec2 sampleLocal = clamp(vec2(0.5) + f, inset, vec2(1.0) - inset);
            // Back out of layer space: where was that layer point actually drawn?
            vec4 c = tapLocal(sampleLocal);

            // THE TILING DOES NOT STOP AT THE LAYER'S BOX. An earlier pass that moves
            // the picture — Oscillate, Twitch, Swing, Transform — slides it partly OUT
            // of that box, and the part that left is simply missing from the scratch at
            // the point this tile asks for: every tile came back with a bite out of it,
            // reported as "kalau di layer ada motion tiles dan di layer itu di tambahkan
            // salah satu dari effect di motion, gambar/object nya akan terpotong".
            //
            // But the plane is PERIODIC, so what left one side of the box is exactly
            // what should arrive at the other: it is sitting one tile over, where the
            // earlier pass put it. Looking there turns the gaps into a seamless,
            // endlessly scrolling tiled field — which is what "move a tiled plane"
            // means, and what After Effects gives you for the same effect order.
            //
            // Safe on its own: outside the layer's box the scratch holds NOTHING unless
            // some pass moved pixels there, so a layer with transparent holes and no
)KF"
R"KF(            // motion under it finds nothing and keeps its holes. uNeighbourFill is
            // therefore only about cost — 8 extra taps on a pixel that came back empty.
            if (uNeighbourFill > 0.5) {
                for (int j = -1; j <= 1; j++) {
                    for (int i = -1; i <= 1; i++) {
                        if (c.a <= 0.0) {
                            vec4 n = tapLocal(sampleLocal + vec2(float(i), float(j)));
                            if (n.a > 0.0) c = n;
                        }
                    }
                }
            }
            // Premultiplied throughout, so a folded Transform's opacity is one scale.
            gl_FragColor = c * (1.0 - uFoldFade);
        }

)KF"
    },
    { "pixelate_cells",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;   // 1/w, 1/h of the target
        uniform vec4 uLayerRect;   // the layer's box in target uv, for the vignette
        uniform float uCellSize;
        uniform float uGap;
        uniform float uRoundCells;
        uniform float uShade;
        uniform float uVignette;
        uniform float uBackEnabled;
        uniform float uBackR;
        uniform float uBackG;
        uniform float uBackB;
        varying vec2 vTex;

        void main() {

            float aspect = uTexelSize.y / uTexelSize.x;   // width / height
            vec2 toUnits = vec2(aspect, 1.0);
            vec2 p = (vTex - 0.5) * toUnits;
            float cell = max(uCellSize * (1.0 / 1080.0), uTexelSize.y);

            vec2 g = p / cell;
            vec2 idx = floor(g);
            // This fragment inside its cell, -0.5..0.5 each way.
            vec2 c = fract(g) - 0.5;

            // The cell's colour is its AVERAGE — sixteen stratified taps, a box
            // filter that holds still while the footage moves. See the doc comment.
            vec2 centreUv = (idx + 0.5) * cell / toUnits + 0.5;
            vec2 cellUv = vec2(cell) / toUnits;
            vec4 sum = vec4(0.0);
            for (int j = 0; j < 4; j++) {
                for (int i = 0; i < 4; i++) {
                    vec2 o = (vec2(float(i), float(j)) + 0.5) / 4.0 - 0.5;
                    sum += texture2D(uTexture, centreUv + o * cellUv);
                }
            }
            // Premultiplied in, premultiplied out: averaging premultiplied colour IS
            // the box filter, and a half-covered cell comes out half transparent.
            vec4 avg = sum / 16.0;

            // The lit part of the cell: a square or a disc, inset by the gap.
            float inner = 0.5 * (1.0 - clamp(uGap, 0.0, 0.95));
            float dist = mix(max(abs(c.x), abs(c.y)), length(c), step(0.5, uRoundCells));
            // Half a texel of antialiasing each side, in cell units.
            float aa = 0.5 * uTexelSize.y / cell;
            float shape = 1.0 - smoothstep(inner - aa, inner + aa, dist);
            // The LED's falloff: brightest in the middle, Cell Shade darker at the rim.
            float t = clamp(dist / max(inner, 1e-4), 0.0, 1.0);
            float lit = shape * (1.0 - clamp(uShade, 0.0, 1.0) * t * t);

            vec4 outc;
            if (uBackEnabled > 0.5) {
                // The panel behind the LEDs: gaps and rim show the backing colour.
                // Scaled by the cell's coverage, so it is painted inside the object's
                // own (blocky) silhouette and never across the layer's box.
                vec3 back = vec3(uBackR, uBackG, uBackB) * avg.a;
                outc = vec4(mix(back, avg.rgb, lit), avg.a);
            } else {
                // No panel: transparent gaps, a rim that fades out. Scaling all four
                // components keeps the result premultiplied.
                outc = avg * lit;
            }

            // Vignette over the layer's box — the Vignette effect's maths, under its
            // rule: RGB only, never alpha, or the layer underneath would show through
            // the darkened corners instead of the picture going dark.
            vec2 vc = (uLayerRect.xy + uLayerRect.zw) * 0.5;
            vec2 halfBox = max((uLayerRect.zw - uLayerRect.xy) * 0.5, vec2(1e-4));
            float d = length((vTex - vc) / halfBox);
            float vig = 1.0 - clamp(uVignette, 0.0, 1.0) * smoothstep(0.5, 1.35, d);
            gl_FragColor = vec4(outc.rgb * vig, outc.a);
        }

)KF"
    },
    { "scanlines",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;   // 1/w, 1/h of the target
        uniform float uInvert;
        uniform float uWidth;
        uniform float uIntensity;
        uniform float uBlend;
        varying vec2 vTex;

        void main() {
            vec4 src = texture2D(uTexture, vTex);
            float a = src.a;

            float aspect = uTexelSize.y / uTexelSize.x;   // width / height
            // Frame-height units, x divided by the aspect so the kernel stays square
            // on a 16:9 comp — a horizontal edge and a vertical one of the same
            // contrast must come back the same weight.
            vec2 span = vec2(1.0 / aspect, 1.0) * (uWidth * (1.0 / 1080.0));
            // Never below one texel of THIS target. In a half-resolution preview the
            // frame-height distance lands inside a texel, and a sub-texel Sobel reads
            // the same bilinear sample twice and returns zero — the effect would
            // simply vanish while scrubbing and reappear in the export.
            vec2 d = max(span, uTexelSize);

            // Premultiplied, deliberately — see the doc comment. Eight taps; Sobel
            // gives the centre weight 0, so it is never fetched for the kernel.
            vec3 c00 = texture2D(uTexture, vTex + vec2(-d.x, -d.y)).rgb;
            vec3 c10 = texture2D(uTexture, vTex + vec2( 0.0, -d.y)).rgb;
            vec3 c20 = texture2D(uTexture, vTex + vec2( d.x, -d.y)).rgb;
            vec3 c01 = texture2D(uTexture, vTex + vec2(-d.x,  0.0)).rgb;
            vec3 c21 = texture2D(uTexture, vTex + vec2( d.x,  0.0)).rgb;
            vec3 c02 = texture2D(uTexture, vTex + vec2(-d.x,  d.y)).rgb;
            vec3 c12 = texture2D(uTexture, vTex + vec2( 0.0,  d.y)).rgb;
            vec3 c22 = texture2D(uTexture, vTex + vec2( d.x,  d.y)).rgb;

            // Sobel, per channel rather than on a luminance: that is what makes the
            // edges carry the colour of the transition.
            vec3 gx = (c20 + 2.0 * c21 + c22) - (c00 + 2.0 * c01 + c02);
            vec3 gy = (c02 + 2.0 * c12 + c22) - (c00 + 2.0 * c10 + c20);
            vec3 g = clamp(sqrt(gx * gx + gy * gy) * uIntensity, 0.0, 1.0);

            // AE: default is dark lines on white, Invert is bright lines on black.
            vec3 edge = mix(1.0 - g, g, step(0.5, uInvert));

            vec3 straight = src.rgb / max(a, 1e-4);
            vec3 outRgb = clamp(mix(edge, straight, uBlend), 0.0, 1.0);
            // Coverage is untouched, and the ground is multiplied by it: the drawing
            // lands inside the artwork's own silhouette, never across its box.
            gl_FragColor = vec4(outRgb * a, a);
        }

)KF"
    },
    { "shape_circle",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;
        uniform mat3 uLayerToLocal;
        uniform float uCenterX;
        uniform float uCenterY;
        uniform float uCount;
        uniform float uWidth;
        uniform float uRotation;
        uniform float uFeather;
        uniform float uInnerRadius;
        uniform float uRadius;
        uniform float uSoftness;
        uniform float uColorR;
        uniform float uColorG;
        uniform float uColorB;
        uniform float uOpacity;
        uniform float uBgFill;
        uniform float uBgR;
        uniform float uBgG;
        uniform float uBgB;
        uniform float uFitAlpha;
        varying vec2 vTex;


float fitCoverage(float baseAlpha) {
    return mix(1.0, baseAlpha, step(0.5, uFitAlpha));
}


float lineCoverage(float d, float hw, float w) {
    w = max(w, 1e-6);
    float lo = max(d - 0.5 * w, -hw);
    float hi = min(d + 0.5 * w, hw);
    return clamp((hi - lo) / w, 0.0, 1.0);
}


        void main() {
            vec4 base = texture2D(uTexture, vTex);

vec3 q = uLayerToLocal * vec3(vTex, 1.0);
if (abs(q.z) < 0.001) {
    gl_FragColor = base;
    return;
}
float s = q.x / q.z;
float t = q.y / q.z;
vec2 row0 = vec2(uLayerToLocal[0][0], uLayerToLocal[1][0]);
vec2 row1 = vec2(uLayerToLocal[0][1], uLayerToLocal[1][1]);
vec2 row2 = vec2(uLayerToLocal[0][2], uLayerToLocal[1][2]);
// One screen pixel, in s and in t, at THIS fragment.
float pxS = length((row0 - s * row2) / q.z * uTexelSize);
float pxT = length((row1 - t * row2) / q.z * uTexelSize);
// The layer's size on screen, in pixels, at its centre (w == 1 there).
float layerW = 1.0 / max(length((row0 - 0.5 * row2) * uTexelSize), 1e-6);
float layerH = 1.0 / max(length((row1 - 0.5 * row2) * uTexelSize), 1e-6);
float layerAspect = layerW / layerH;
vec2 p = vec2(s * layerAspect, t);
float px = max(pxS * layerAspect, pxT);
// One composition pixel at 1080p (the convention Pixelate and Find Edges use), in
// pattern units: frame height over layer height, both as they stand on THIS target,
// so a "1 px" border is the same picture at preview resolution and in the export,
// and stays 1 px when the layer is scaled.
float pxUnit = 1.0 / (uTexelSize.y * layerH * 1080.0);

float hS = 0.5 * pxS;
float hT = 0.5 * pxT;
float insideQuad = smoothstep(-hS, hS, s) * (1.0 - smoothstep(1.0 - hS, 1.0 + hS, s))
                 * smoothstep(-hT, hT, t) * (1.0 - smoothstep(1.0 - hT, 1.0 + hT, t));

float inside = mix(insideQuad, 1.0, step(0.5, uFitAlpha));
float cover = fitCoverage(base.a);
if (inside <= 0.0 || cover <= 0.0) {
    gl_FragColor = base;
    return;
}


            vec2 c = p - vec2(uCenterX * layerAspect, uCenterY);
            float r = length(c);
            float count = max(floor(uCount + 0.5), 1.0);
            float per = 6.2831853 / count;
            // atan(0, 0) is undefined; the centre pixel takes angle 0.
            float ang = (r > 1e-6) ? atan(c.y, c.x) : 0.0;
            ang -= radians(uRotation);
            float f = fract(ang / per);             // 0..1 across one ray and gap
            // Arc length from the ray's centre line, and the ray's half-width in the
            // same units: this is where a ray's edge gets its one-pixel antialiasing.
            float arc = abs(f - 0.5) * per * r;
            float hw = clamp(uWidth, 0.0, 1.0) * 0.5 * per * r;
            float m = lineCoverage(arc, hw, px + uFeather * per * r);

            // Radial extent: the hole in the middle and the (soft or hard) outer end.
            float hp = 0.5 * px;
            m *= smoothstep(uInnerRadius - hp, uInnerRadius + hp, r);
            float rad = max(uRadius, 1e-4);
            m *= 1.0 - smoothstep(rad * (1.0 - clamp(uSoftness, 0.0, 1.0)) - hp, rad + hp, r);

float a = clamp(m, 0.0, 1.0) * clamp(uOpacity, 0.0, 1.0);
vec4 paint = vec4(uColorR, uColorG, uColorB, 1.0) * a;
vec4 bg = vec4(uBgR, uBgG, uBgB, 1.0) * step(0.5, uBgFill);
vec4 pattern = (bg * (1.0 - a) + paint) * cover;
gl_FragColor = mix(base, pattern, inside);

        }

)KF"
    },
    { "shape_ellipse",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;
        uniform mat3 uLayerToLocal;
        uniform float uWidth;
        uniform float uHeight;
        uniform float uAnchorX;
        uniform float uAnchorY;
        uniform float uRotation;
        uniform float uFeather;
        uniform float uInvert;
        uniform float uColorR;
        uniform float uColorG;
        uniform float uColorB;
        uniform float uOpacity;
        uniform float uBgFill;
        uniform float uBgR;
        uniform float uBgG;
        uniform float uBgB;
        uniform float uFitAlpha;
        varying vec2 vTex;


float fitCoverage(float baseAlpha) {
    return mix(1.0, baseAlpha, step(0.5, uFitAlpha));
}


        void main() {
            vec4 base = texture2D(uTexture, vTex);

vec3 q = uLayerToLocal * vec3(vTex, 1.0);
if (abs(q.z) < 0.001) {
    gl_FragColor = base;
    return;
}
float s = q.x / q.z;
float t = q.y / q.z;
vec2 row0 = vec2(uLayerToLocal[0][0], uLayerToLocal[1][0]);
vec2 row1 = vec2(uLayerToLocal[0][1], uLayerToLocal[1][1]);
vec2 row2 = vec2(uLayerToLocal[0][2], uLayerToLocal[1][2]);
// One screen pixel, in s and in t, at THIS fragment.
float pxS = length((row0 - s * row2) / q.z * uTexelSize);
float pxT = length((row1 - t * row2) / q.z * uTexelSize);
// The layer's size on screen, in pixels, at its centre (w == 1 there).
float layerW = 1.0 / max(length((row0 - 0.5 * row2) * uTexelSize), 1e-6);
float layerH = 1.0 / max(length((row1 - 0.5 * row2) * uTexelSize), 1e-6);
float layerAspect = layerW / layerH;
vec2 p = vec2(s * layerAspect, t);
float px = max(pxS * layerAspect, pxT);
// One composition pixel at 1080p (the convention Pixelate and Find Edges use), in
// pattern units: frame height over layer height, both as they stand on THIS target,
// so a "1 px" border is the same picture at preview resolution and in the export,
// and stays 1 px when the layer is scaled.
float pxUnit = 1.0 / (uTexelSize.y * layerH * 1080.0);

float hS = 0.5 * pxS;
float hT = 0.5 * pxT;
float insideQuad = smoothstep(-hS, hS, s) * (1.0 - smoothstep(1.0 - hS, 1.0 + hS, s))
                 * smoothstep(-hT, hT, t) * (1.0 - smoothstep(1.0 - hT, 1.0 + hT, t));

float inside = mix(insideQuad, 1.0, step(0.5, uFitAlpha));
float cover = fitCoverage(base.a);
if (inside <= 0.0 || cover <= 0.0) {
    gl_FragColor = base;
    return;
}


            vec2 c = p - vec2(uAnchorX * layerAspect, uAnchorY);
            float ca = cos(radians(uRotation));
            float sa = sin(radians(uRotation));
            c = vec2(c.x * ca - c.y * sa, c.x * sa + c.y * ca);

            vec2 cell = max(vec2(uWidth, uHeight), vec2(1e-4));
            vec2 g = c / cell;
            vec2 i = floor(g);
            vec2 f = (g - i) * cell;                // where in the cell, height units
            vec2 d = min(f, cell - f);              // distance to the nearest edge, per axis
            // mod() is never negative in GLSL, so the parity holds left of the anchor.
            vec2 parity = 1.0 - 2.0 * mod(i, 2.0);
            float w = 0.5 * px + uFeather * pxUnit;
            vec2 ramp = clamp(d / w, 0.0, 1.0) * parity;
            float m = 0.5 + 0.5 * ramp.x * ramp.y;
            m = mix(m, 1.0 - m, step(0.5, uInvert));

float a = clamp(m, 0.0, 1.0) * clamp(uOpacity, 0.0, 1.0);
vec4 paint = vec4(uColorR, uColorG, uColorB, 1.0) * a;
vec4 bg = vec4(uBgR, uBgG, uBgB, 1.0) * step(0.5, uBgFill);
vec4 pattern = (bg * (1.0 - a) + paint) * cover;
gl_FragColor = mix(base, pattern, inside);

        }

)KF"
    },
    { "shape_radial_repeat",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;
        uniform mat3 uLayerToLocal;
        uniform float uSize;
        uniform float uAnchorX;
        uniform float uAnchorY;
        uniform float uRotation;
        uniform float uBorder;
        uniform float uFeather;
        uniform float uInvert;
        uniform float uColorR;
        uniform float uColorG;
        uniform float uColorB;
        uniform float uOpacity;
        uniform float uBgFill;
        uniform float uBgR;
        uniform float uBgG;
        uniform float uBgB;
        uniform float uFitAlpha;
        varying vec2 vTex;


float fitCoverage(float baseAlpha) {
    return mix(1.0, baseAlpha, step(0.5, uFitAlpha));
}


float lineCoverage(float d, float hw, float w) {
    w = max(w, 1e-6);
    float lo = max(d - 0.5 * w, -hw);
    float hi = min(d + 0.5 * w, hw);
    return clamp((hi - lo) / w, 0.0, 1.0);
}


        void main() {
            vec4 base = texture2D(uTexture, vTex);

vec3 q = uLayerToLocal * vec3(vTex, 1.0);
if (abs(q.z) < 0.001) {
    gl_FragColor = base;
    return;
}
float s = q.x / q.z;
float t = q.y / q.z;
vec2 row0 = vec2(uLayerToLocal[0][0], uLayerToLocal[1][0]);
vec2 row1 = vec2(uLayerToLocal[0][1], uLayerToLocal[1][1]);
vec2 row2 = vec2(uLayerToLocal[0][2], uLayerToLocal[1][2]);
// One screen pixel, in s and in t, at THIS fragment.
float pxS = length((row0 - s * row2) / q.z * uTexelSize);
float pxT = length((row1 - t * row2) / q.z * uTexelSize);
// The layer's size on screen, in pixels, at its centre (w == 1 there).
float layerW = 1.0 / max(length((row0 - 0.5 * row2) * uTexelSize), 1e-6);
float layerH = 1.0 / max(length((row1 - 0.5 * row2) * uTexelSize), 1e-6);
float layerAspect = layerW / layerH;
vec2 p = vec2(s * layerAspect, t);
float px = max(pxS * layerAspect, pxT);
// One composition pixel at 1080p (the convention Pixelate and Find Edges use), in
// pattern units: frame height over layer height, both as they stand on THIS target,
// so a "1 px" border is the same picture at preview resolution and in the export,
// and stays 1 px when the layer is scaled.
float pxUnit = 1.0 / (uTexelSize.y * layerH * 1080.0);

float hS = 0.5 * pxS;
float hT = 0.5 * pxT;
float insideQuad = smoothstep(-hS, hS, s) * (1.0 - smoothstep(1.0 - hS, 1.0 + hS, s))
                 * smoothstep(-hT, hT, t) * (1.0 - smoothstep(1.0 - hT, 1.0 + hT, t));

float inside = mix(insideQuad, 1.0, step(0.5, uFitAlpha));
float cover = fitCoverage(base.a);
if (inside <= 0.0 || cover <= 0.0) {
    gl_FragColor = base;
    return;
}


            vec2 c = p - vec2(uAnchorX * layerAspect, uAnchorY);
            float ca = cos(radians(uRotation));
            float sa = sin(radians(uRotation));
            c = vec2(c.x * ca - c.y * sa, c.x * sa + c.y * ca);

            float size = max(uSize, 1e-4);          // flat-to-flat width of a cell
            vec2 g = c / size;
            // Two interleaved lattices, a half cell apart each way; the nearer centre
            // of the two is this fragment's cell. Pointy-top, inradius 0.5.
            vec2 r = vec2(1.0, 1.7320508);
            vec2 h = 0.5 * r;
            vec2 a1 = mod(g, r) - h;
            vec2 a2 = mod(g - h, r) - h;
            vec2 gv = (dot(a1, a1) < dot(a2, a2)) ? a1 : a2;
            // Distance to the nearest edge: inradius minus the largest projection onto
            // an edge normal — (1, 0) for the vertical sides, (cos 60, sin 60) for the
            // slanted ones; abs() folds the other four onto these two.
            vec2 ag = abs(gv);
            float hexDist = max(dot(ag, vec2(0.5, 0.8660254)), ag.x);
            float e = (0.5 - hexDist) * size;       // height units
            float m = lineCoverage(e, 0.5 * uBorder * pxUnit, px + uFeather * pxUnit);
            m = mix(m, 1.0 - m, step(0.5, uInvert));

float a = clamp(m, 0.0, 1.0) * clamp(uOpacity, 0.0, 1.0);
vec4 paint = vec4(uColorR, uColorG, uColorB, 1.0) * a;
vec4 bg = vec4(uBgR, uBgG, uBgB, 1.0) * step(0.5, uBgFill);
vec4 pattern = (bg * (1.0 - a) + paint) * cover;
gl_FragColor = mix(base, pattern, inside);

        }

)KF"
    },
    { "shape_rect",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;
        uniform mat3 uLayerToLocal;
        uniform float uWidth;
        uniform float uHeight;
        uniform float uAnchorX;
        uniform float uAnchorY;
        uniform float uRotation;
        uniform float uBorder;
        uniform float uFeather;
        uniform float uInvert;
        uniform float uColorR;
        uniform float uColorG;
        uniform float uColorB;
        uniform float uOpacity;
        uniform float uBgFill;
        uniform float uBgR;
        uniform float uBgG;
        uniform float uBgB;
        uniform float uFitAlpha;
        varying vec2 vTex;


float fitCoverage(float baseAlpha) {
    return mix(1.0, baseAlpha, step(0.5, uFitAlpha));
}


float lineCoverage(float d, float hw, float w) {
    w = max(w, 1e-6);
    float lo = max(d - 0.5 * w, -hw);
    float hi = min(d + 0.5 * w, hw);
    return clamp((hi - lo) / w, 0.0, 1.0);
}


        void main() {
            vec4 base = texture2D(uTexture, vTex);

vec3 q = uLayerToLocal * vec3(vTex, 1.0);
if (abs(q.z) < 0.001) {
    gl_FragColor = base;
    return;
}
float s = q.x / q.z;
float t = q.y / q.z;
vec2 row0 = vec2(uLayerToLocal[0][0], uLayerToLocal[1][0]);
vec2 row1 = vec2(uLayerToLocal[0][1], uLayerToLocal[1][1]);
vec2 row2 = vec2(uLayerToLocal[0][2], uLayerToLocal[1][2]);
// One screen pixel, in s and in t, at THIS fragment.
float pxS = length((row0 - s * row2) / q.z * uTexelSize);
float pxT = length((row1 - t * row2) / q.z * uTexelSize);
// The layer's size on screen, in pixels, at its centre (w == 1 there).
float layerW = 1.0 / max(length((row0 - 0.5 * row2) * uTexelSize), 1e-6);
float layerH = 1.0 / max(length((row1 - 0.5 * row2) * uTexelSize), 1e-6);
float layerAspect = layerW / layerH;
vec2 p = vec2(s * layerAspect, t);
float px = max(pxS * layerAspect, pxT);
// One composition pixel at 1080p (the convention Pixelate and Find Edges use), in
// pattern units: frame height over layer height, both as they stand on THIS target,
// so a "1 px" border is the same picture at preview resolution and in the export,
// and stays 1 px when the layer is scaled.
float pxUnit = 1.0 / (uTexelSize.y * layerH * 1080.0);

float hS = 0.5 * pxS;
float hT = 0.5 * pxT;
float insideQuad = smoothstep(-hS, hS, s) * (1.0 - smoothstep(1.0 - hS, 1.0 + hS, s))
                 * smoothstep(-hT, hT, t) * (1.0 - smoothstep(1.0 - hT, 1.0 + hT, t));

float inside = mix(insideQuad, 1.0, step(0.5, uFitAlpha));
float cover = fitCoverage(base.a);
if (inside <= 0.0 || cover <= 0.0) {
    gl_FragColor = base;
    return;
}


            // Pattern space about the anchor, turned by Rotation.
            vec2 c = p - vec2(uAnchorX * layerAspect, uAnchorY);
            float ca = cos(radians(uRotation));
            float sa = sin(radians(uRotation));
            c = vec2(c.x * ca - c.y * sa, c.x * sa + c.y * ca);

            vec2 cell = max(vec2(uWidth, uHeight), vec2(1e-4));
            vec2 f = fract(c / cell) * cell;        // where in the cell, height units
            vec2 d = min(f, cell - f);              // distance to the nearest line, per axis
            float w = px + uFeather * pxUnit;
            float hw = 0.5 * uBorder * pxUnit;
            float lx = lineCoverage(d.x, hw, w);
            float ly = lineCoverage(d.y, hw, w);
            // Union of the two line families, so a crossing is not twice as bright.
            float m = 1.0 - (1.0 - lx) * (1.0 - ly);
            m = mix(m, 1.0 - m, step(0.5, uInvert));

float a = clamp(m, 0.0, 1.0) * clamp(uOpacity, 0.0, 1.0);
vec4 paint = vec4(uColorR, uColorG, uColorB, 1.0) * a;
vec4 bg = vec4(uBgR, uBgG, uBgB, 1.0) * step(0.5, uBgFill);
vec4 pattern = (bg * (1.0 - a) + paint) * cover;
gl_FragColor = mix(base, pattern, inside);

        }

)KF"
    },
    { "sharpen",
R"KF(precision mediump float;
uniform sampler2D uTexture;   // the layer as this effect received it
uniform vec2 uTexelSize;
uniform float uEdgeThickness;
varying vec2 vTex;
const int K = 8;
void main() {
    float aspect = uTexelSize.y / uTexelSize.x;
    float step = max(uEdgeThickness, 1e-4) / float(K) / aspect;
    float sum = 0.0;
    float wsum = 0.0;
    for (int i = -K; i <= K; i++) {
        float w = exp(-3.0 * float(i * i) / float(K * K));
        sum += texture2D(uTexture, vTex + vec2(float(i) * step, 0.0)).a * w;
        wsum += w;
    }
    gl_FragColor = vec4(0.0, 0.0, 0.0, sum / wsum);
}

)KF"
    },
    { "solid_color",
R"KF(precision mediump float;
uniform sampler2D uTexture;
uniform float uColorR;
uniform float uColorG;
uniform float uColorB;
uniform float uOpacity;
varying vec2 vTex;
void main() {
    vec4 c = texture2D(uTexture, vTex);
    // The layer arrives premultiplied; mixing colours needs straight RGB,
    // otherwise a semi-transparent edge pulls the fill toward black.
    float a = c.a;
    vec3 rgb = c.rgb / max(a, 1e-4);
    rgb = mix(rgb, vec3(uColorR, uColorG, uColorB), uOpacity);
    gl_FragColor = vec4(rgb * a, a);
}

)KF"
    },
    { "stroke_glow",
R"KF(precision mediump float;
uniform sampler2D uTexture;   // pass-1 output (alpha in .a)
uniform sampler2D uInput;     // the stack's state as this effect received it
uniform vec2 uTexelSize;
uniform float uColorR;
uniform float uColorG;
uniform float uColorB;
uniform float uWidth;
varying vec2 vTex;
const int K = 8;
void main() {
    float step = uWidth / float(K);
    float m = 0.0;
    for (int i = -K; i <= K; i++) {
        m = max(m, texture2D(uTexture, vTex + vec2(0.0, float(i) * step)).a);
    }
    vec4 layer = texture2D(uInput, vTex);
    float border = clamp(m - layer.a, 0.0, 1.0);
    vec4 stroke = vec4(vec3(uColorR, uColorG, uColorB) * border, border);
    gl_FragColor = layer + stroke * (1.0 - layer.a);
}

)KF"
    },
    { "tex_copy",
R"KF(precision mediump float;
uniform sampler2D uTexture;
varying vec2 vTex;
void main() { gl_FragColor = texture2D(uTexture, vTex); }

)KF"
    },
    { "transform_2d",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec4 uLayerRect;   // pivot region in 0..1 target space
        uniform vec2 uTexelSize;   // 1/w, 1/h
        uniform float uAnchorX;
        uniform float uAnchorY;
        uniform float uPositionX;
        uniform float uPositionY;
        uniform float uScale;
        uniform float uRotation;
        uniform float uOpacity;
        varying vec2 vTex;


vec4 sampleOrNothing(sampler2D tex, vec2 uv) {
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return vec4(0.0);
    return texture2D(tex, uv);
}


uniform sampler2D uWideSource;
uniform float uWideCopy;
uniform vec4 uWideFit;        // (kx, ky, ox, oy): layer (s, t) -> copy (s*kx+ox, t*ky+oy)
uniform mat3 uLayerToLocal;   // target uv -> (s*w, t*w, w) in the LAYER's space
uniform mat3 uPreMotion;      // layer point -> the layer point drawn there by the passes above
uniform float uPreMotionOn;
uniform float uFoldFade;      // 1 - opacity of the Transforms folded into uPreMotion

vec4 sampleLayer(vec2 uv) {
    if (uWideCopy < 0.5) return sampleOrNothing(uTexture, uv);
    vec3 q = uLayerToLocal * vec3(uv, 1.0);
    // At or past the horizon of a tilted plane there is no layer point (a degenerate
    // matrix never reaches here: the renderer draws no copy for a layer it cannot lay
    // flat).
    if (q.z < 1e-4) return vec4(0.0);
    vec2 p = q.xy / q.z;
    if (uPreMotionOn > 0.5) {
        vec3 m = uPreMotion * vec3(p, 1.0);
        if (m.z <= 0.0) return vec4(0.0);
        p = m.xy / m.z;
    }
    vec4 fit = uWideFit.x > 0.0 ? uWideFit : vec4(1.0, 1.0, 0.0, 0.0);
    // t runs DOWN the layer, the texture's v runs up.
    vec2 c = vec2(p.x * fit.x + fit.z, 1.0 - (p.y * fit.y + fit.w));
    if (c.x < 0.0 || c.x > 1.0 || c.y < 0.0 || c.y > 1.0) return vec4(0.0);
    return texture2D(uWideSource, c) * (1.0 - uFoldFade);
}


        void main() {
            vec2 layerMin = uLayerRect.xy;
            vec2 layerSize = max(uLayerRect.zw - uLayerRect.xy, vec2(1e-4));
            vec2 pivot = layerMin + vec2(uAnchorX, uAnchorY) * layerSize;
            float aspect = uTexelSize.y / uTexelSize.x;   // width / height

            // Invert the transform: for each output pixel, find the source pixel.
            vec2 p = vTex - pivot - vec2(uPositionX, uPositionY);
            p.x *= aspect;                                 // work in a square space
            float a = -uRotation * 0.017453293;
            float c = cos(a);
            float s = sin(a);
            p = mat2(c, -s, s, c) * p;
            p /= max(uScale, 1e-4);
            p.x /= aspect;
            vec2 uv = pivot + p;

            // Nothing outside the source (sampleLayer), never a smeared edge.
            // Premultiplied throughout, so scaling by opacity keeps it premultiplied.
            gl_FragColor = sampleLayer(uv) * clamp(uOpacity, 0.0, 1.0);
        }

)KF"
    },
    { "vs_mvp_stmatrix",
R"KF(uniform mat4 uMVP;
uniform mat4 uSTMatrix;
attribute vec2 aPos;
attribute vec2 aTex;
varying vec2 vTex;
void main() {
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
    vTex = (uSTMatrix * vec4(aTex, 0.0, 1.0)).xy;
}

)KF"
    },
    { "vs_particles",
R"KF(precision highp float;

attribute vec3 aParticle;   // x = slot index, yz = quad corner in -1..1

uniform mat4 uVP;
// The view-projection and the emitter as they were at the START of the shutter
// window. Motion blur is measured against these, so a moving CAMERA and a moving
// EMITTER smear the particles exactly as their own flight does. Equal to the pair
// above when neither moved.
uniform mat4 uVPPrev;
uniform vec3 uOriginPrev;
// HALF the shutter window, in seconds. 0 = the layer's motion-blur switch is off,
// and the whole block below is skipped.
uniform float uShutter;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
uniform vec3 uOrigin;       // emitter, composition space (+Z away, AE sign)
uniform vec2 uComp;         // composition size in px — emitter size is a fraction of it
uniform float uTime;        // seconds since the layer's in point
uniform float uRate;        // particles per second the user asked for (clamped)
uniform float uLattice;     // slots per second — fixed, independent of uRate
uniform float uPool;        // slots in the pool

uniform float uPreRoll;
uniform float uPosX, uPosY, uPosZ;
uniform float uEmitterW, uEmitterH, uEmitterD, uEmitterSphere;
uniform float uVelocity, uVelocityRandom, uDirTilt, uDirSpin, uSpread, uOutwards;
uniform float uGravity, uWindX, uWindY, uWindZ, uDrag, uTurbulence, uTurbSpeed;
uniform float uLife, uLifeRandom, uSize, uSizeRandom, uSizeEnd, uStretch;
uniform float uOpacity, uFadeIn, uFadeOut;
uniform float uColorR, uColorG, uColorB, uColorRandom;
uniform float uColorEndR, uColorEndG, uColorEndB;
uniform float uSeed;

varying vec2 vCorner;
varying vec4 vColor;

const float TAU = 6.2831853;

// Three uncorrelated 0..1 values from one number (Hoskins' hash — cheap, and stable
// across drivers because it is pure float arithmetic with no sin()).
vec3 hash3(float p) {
    vec3 p3 = fract(vec3(p) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yxz + 33.33);
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

vec3 hsv2rgb(vec3 c) {
    vec3 k = abs(fract(c.xxx + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
    return c.z * mix(vec3(1.0), clamp(k - 1.0, 0.0, 1.0), c.y);
}

/**
 * Displacement from the launch point after `dt` seconds of flight — the closed form
 * the whole system rests on, pulled out as a function because motion blur has to
 * evaluate it a SECOND time, at the start of the shutter window. That is the payoff
 * of a stateless simulation: "where was this particle a 200th of a second ago" is a
 * function call, not a history buffer.
 *
 * `damp` is the integral of exp(-drag*t); the series form near zero keeps it finite
 * where the exact expression divides 0 by 0, so drag can be keyframed up from
 * nothing without a discontinuity.
 *
 * Turbulence is NOT here — it is its own function below, because motion blur has to
 * evaluate it at both instants too. See [turbulenceOffset].
 */
vec3 flightOffset(float dt, vec3 dir, float speed) {
    float drag = max(uDrag, 0.0);
    float damp = (drag * dt < 1e-3) ? dt * (1.0 - 0.5 * drag * dt)
                                    : (1.0 - exp(-drag * dt)) / drag;
    return dir * speed * damp
         + vec3(uWindX, uWindY, uWindZ) * dt
         + vec3(0.0, uGravity, 0.0) * (0.5 * dt * dt);
}

/**
 * The turbulent wander, at absolute time [clock] and life fraction [kk].
 *
 * Per-particle wander rather than a sampled noise field: a field would have to be
 * evaluated at the CURRENT position, and the current position is what it is
 * displacing — a loop no closed form can carry. Summed sines at incommensurable rates
 * never repeat over any usable timeline, and each particle gets its own phase, so the
 * spray breaks up instead of swaying as one body. Ramped in over the first half of
 * life so nothing pops at birth.
 *
 * **It is a function of TIME, so motion blur must evaluate it at both ends of the
 * shutter and take the difference.** Leaving it out of the earlier position instead
 * makes the blur count this whole displacement as travel — tens of pixels of
 * per-particle noise, which swamps the few pixels the particle actually moved and
 * sprays streaks in random directions.
 */
vec3 turbulenceOffset(float clock, float kk, vec3 phase, float generation) {
    if (uTurbulence <= 0.0) return vec3(0.0);
    float w = uTurbSpeed * (clock + generation * 3.7);
    vec3 wander = vec3(
        sin(w + phase.x) + 0.5 * sin(2.13 * w + phase.y),
        sin(1.17 * w + phase.y) + 0.5 * sin(2.31 * w + phase.z),
        sin(0.93 * w + phase.z) + 0.5 * sin(1.87 * w + phase.x)
    );
    return uTurbulence * wander * min(kk * 2.0, 1.0);
}

void main() {
    float idx = aParticle.x;
    vec2 corner = aParticle.yz;

    // Which emission this slot is on, and how long ago it started. Births are
    // staggered by index so the stream is continuous rather than one pulse per
    // cycle; the pool is sized (on the CPU) so one cycle outlasts one life.
    // Pre-roll: the simulation is already this far along at the layer's in point.
    // Free, and only a closed-form system can offer it — there is no state to fast
    // forward, so "already running for 2 seconds" is an addition. It is what makes a
    // full screen of particles exist at the FIRST frame instead of building up from
    // an empty frame every time the layer starts.
    float now = uTime + uPreRoll * 0.001;
    float lattice = max(uLattice, 1e-4);
    float cycle = max(uPool / lattice, 1e-4);
    float since = now - idx / lattice;
    float generation = floor(since / cycle);
    float age = since - generation * cycle;

    // Randomness is per (slot, generation): a respawned particle is a NEW particle,
    // not the same one replayed.
    float sid = idx * 1.7 + generation * 91.7 + uSeed * 13.31;
    vec3 r1 = hash3(sid);           // life, cone
    vec3 r2 = hash3(sid + 37.13);   // emitter offset
    vec3 r3 = hash3(sid + 71.77);   // sphere direction, turbulence phase
    vec3 r4 = hash3(sid + 113.7);   // speed, size, hue
    vec3 r5 = hash3(sid + 191.3);   // emission lottery

    float life = max(uLife, 1.0) * 0.001 * (1.0 + (r1.x - 0.5) * 2.0 * uLifeRandom);
    life = max(life, 1e-3);
    float k = age / life;

    // The rate as a fraction of the lattice, drawn against this slot's own fixed
    // number: the slots stay where they are and the rate only decides how many of
    // them fire. That is what lets Particles/second be keyframed without the whole
    // spray re-timing itself, and it makes the emission irregular the way a real
    // spray is, rather than a metronome ticking at exactly 1/rate.
    bool emitted = r5.x < uRate / lattice;

    // Never fired, not born yet, or already dead: collapse behind the near plane so
    // the quad is clipped away entirely. (All six vertices agree — same slot index.)
    if (!emitted || since < 0.0 || k > 1.0) {
        gl_Position = vec4(0.0, 0.0, -2.0, 1.0);
        vColor = vec4(0.0);
        vCorner = vec2(0.0);
        return;
    }

)KF"
R"KF(    // Where in the emitter it starts. Zero size collapses to a point emitter; the
    // size is a FRACTION OF THE FRAME, so 100% is exactly one screen across and a
    // project re-rendered at another resolution emits over the same region. (Depth
    // has no frame dimension of its own and uses the height, the same convention
    // Wave Warp and Bulge measure by.)
    vec3 halfSize = vec3(uEmitterW * uComp.x, uEmitterH * uComp.y, uEmitterD * uComp.y) * 0.5;
    vec3 unit = r2 * 2.0 - 1.0;
    if (uEmitterSphere > 0.5) {
        // Uniform on the sphere via the cos-z / phi construction, then pushed to a
        // uniform density inside it. Normalising a random cube point instead piles
        // particles up at the eight corners, which reads as a visibly lumpy ball.
        float cz = r3.x * 2.0 - 1.0;
        float sr = sqrt(max(0.0, 1.0 - cz * cz));
        float ph = TAU * r3.y;
        unit = vec3(sr * cos(ph), sr * sin(ph), cz) * pow(max(r2.x, 1e-4), 1.0 / 3.0);
    }
    vec3 offset = unit * halfSize;

    // Emission direction: a cone of half-angle uSpread around the aim. Composition
    // space is y-DOWN, so "up the screen" is -Y and a tilt of 0 is straight up.
    float tilt = radians(uDirTilt);
    float spin = radians(uDirSpin);
    vec3 aim = vec3(sin(tilt) * sin(spin), -cos(tilt), sin(tilt) * cos(spin));
    // Uniform over the spherical cap: the COSINE is what must be uniform, not the
    // angle, or the spray bunches along its axis.
    float cosMax = cos(radians(clamp(uSpread, 0.0, 180.0)));
    float cz2 = mix(1.0, cosMax, r1.y);
    float sz2 = sqrt(max(0.0, 1.0 - cz2 * cz2));
    float ph2 = TAU * r1.z;
    vec3 pole = abs(aim.y) > 0.99 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 tAxis = normalize(cross(pole, aim));
    vec3 bAxis = cross(aim, tAxis);
    vec3 dir = normalize(tAxis * (sz2 * cos(ph2)) + bAxis * (sz2 * sin(ph2)) + aim * cz2);

    float offLen = length(offset);
    if (uOutwards > 0.001 && offLen > 0.001) {
        dir = normalize(mix(dir, offset / offLen, clamp(uOutwards, 0.0, 1.0)));
    }

    float speed = uVelocity * max(0.0, 1.0 + (r4.x - 0.5) * 2.0 * uVelocityRandom);

    float dt = age;
    vec3 phase = r3 * TAU;
    vec3 emitterOffset = vec3(uPosX * uComp.x, uPosY * uComp.y, uPosZ * uComp.y) + offset;
    vec3 pos = uOrigin + emitterOffset + flightOffset(dt, dir, speed)
             + turbulenceOffset(now, k, phase, generation);

    float size = uSize * max(0.05, 1.0 + (r4.y - 0.5) * 2.0 * uSizeRandom)
               * mix(1.0, uSizeEnd, k);
    float fadeIn = uFadeIn <= 0.0 ? 1.0 : smoothstep(0.0, uFadeIn, k);
    float fadeOut = uFadeOut <= 0.0 ? 1.0 : 1.0 - smoothstep(1.0 - uFadeOut, 1.0, k);

    // Colour travels from its start to its end across the particle's own life —
    // which is what an ember cooling from white through orange to red IS. Both
    // swatches white leaves it a plain constant colour, so it costs nothing until
    // the second one is moved.
    vec3 color = mix(vec3(uColorR, uColorG, uColorB), vec3(uColorEndR, uColorEndG, uColorEndB), k);
    if (uColorRandom > 0.0) {
        color = mix(color, hsv2rgb(vec3(r4.z, 0.85, 1.0)), clamp(uColorRandom, 0.0, 1.0));
    }
    vColor = vec4(color, uOpacity * fadeIn * fadeOut);
    vCorner = corner;

    // Billboard axes. Normally the camera's own right/up, so the sprite always faces
    // the lens; with Stretch, the quad is turned to lie ALONG the particle's motion
    // and lengthened by how fast it is going, which is what turns a dot into a spark
    // or a raindrop. The velocity is the analytic derivative of the position above,
    // so it costs no extra state.
    vec3 axisU = uCamRight;
    vec3 axisV = uCamUp;
    float sizeV = size;
    if (uStretch > 0.0) {
        vec3 vel = dir * speed * exp(-max(uDrag, 0.0) * dt)
                 + vec3(uWindX, uWindY, uWindZ)
                 + vec3(0.0, uGravity, 0.0) * dt;
        // Only the part of the velocity the camera can SEE stretches the sprite:
        // one flying straight at the lens has no screen direction to stretch along,
        // and must stay a dot rather than smear in an arbitrary direction.
        vec2 vs = vec2(dot(vec3(vel.x, vel.y, -vel.z), uCamRight),
                       dot(vec3(vel.x, vel.y, -vel.z), uCamUp));
        float vlen = length(vs);
        if (vlen > 1e-3) {
            vec2 d = vs / vlen;
            axisV = uCamRight * d.x + uCamUp * d.y;    // along the motion
            axisU = uCamRight * -d.y + uCamUp * d.x;   // across it
            // Measured against the launch speed, so the amount means the same thing
            // whatever units the scene is built in. Clamped so a particle caught in
            // a hurricane wind does not become a line across the frame.
            sizeV = size * clamp(1.0 + uStretch * vlen / max(uVelocity, 1.0),
                                 1.0, 1.0 + uStretch * 6.0);
        }
    }

    // Composition Z is AE's (+Z away); GL's is the opposite.
    vec3 world = vec3(pos.x, pos.y, -pos.z);

    // ── Motion blur ────────────────────────────────────────────────────────────
    // Per PARTICLE, not per layer, because that is the only place the motion is:
    // a particle system's movement lives in the particles, so the layer's transform
    // — which is what the compositor's velocity-blur pass measures — is perfectly
    // still while the frame is full of streaks.
    //
    // The apparent travel is measured where it is actually seen: on SCREEN, between
    // the particle's own position a shutter ago through the camera of that instant,
    // and its position now through the camera of now. One subtraction therefore
    // carries all three sources at once — the particle's flight, a moving emitter,
    // and a moving camera, the last of them WITH parallax (a mote near the lens
    // smears further under a pan than one far away, which a single whole-layer
    // velocity field cannot express).
    vec4 cNow = uVP * vec4(world, 1.0);
    // Skipped for anything at or behind the lens: w is the distance in front of the
    // camera, and dividing by a w near zero throws the projected position off to
    // infinity — the difference of two such numbers is noise, and noise here means a
    // streak pointing somewhere the particle never went.
    if (uShutter > 0.0 && cNow.w > uComp.y * 0.02) {
        float dtPrev = max(dt - uShutter, 0.0);
        vec3 posPrev = uOriginPrev + emitterOffset
                     + flightOffset(dtPrev, dir, speed)
                     // Evaluated at the EARLIER instant, so only the turbulence's
                     // CHANGE across the shutter counts as travel. Reusing the
                     // current wander here would hand the blur tens of pixels of
                     // per-particle noise as if the particle had moved that far.
                     + turbulenceOffset(now - uShutter, dtPrev / life, phase, generation);
)KF"
R"KF(        vec4 cPrev = uVPPrev * vec4(vec3(posPrev.x, posPrev.y, -posPrev.z), 1.0);
        if (cPrev.w > uComp.y * 0.02) {
            vec2 nNow = cNow.xy / cNow.w;
            // Doubled: uShutter is the HALF window, and the camera is interpolated
            // linearly across it, so start->frame is exactly half of start->end.
            vec2 dNdc = (nNow - cPrev.xy / cPrev.w) * 2.0;
            // Screen motion back into world units, measured against the camera's own
            // axes AT THIS DEPTH. A world step along uCamRight lands on clip x alone
            // and uCamUp on clip y alone — that is what makes them the camera's axes
            // — so the two scales are independent and no general solve is needed,
            // and the y flip in the projection is accounted for by construction.
            vec4 cU = uVP * vec4(world + uCamRight, 1.0);
            vec4 cV = uVP * vec4(world + uCamUp, 1.0);
            float perU = cU.x / cU.w - nNow.x;
            float perV = cV.y / cV.w - nNow.y;
            vec2 travel = vec2(dNdc.x / (abs(perU) < 1e-9 ? 1e-9 : perU),
                               dNdc.y / (abs(perV) < 1e-9 ? 1e-9 : perV));
            // Capped at a frame height: past that the streak is neither believable
            // nor affordable — it is a full-frame quad per particle.
            float mbLen = min(length(travel), uComp.y);
            if (mbLen > max(size, 1.0) * 0.05) {
                vec2 a = normalize(travel);
                axisV = uCamRight * a.x + uCamUp * a.y;   // along the apparent motion
                axisU = uCamRight * -a.y + uCamUp * a.x;  // across it
                sizeV += mbLen;
                // A real shutter spreads the SAME light over the whole streak, so a
                // sprite drawn n times longer must be n times dimmer. Without this,
                // turning motion blur on makes a fast spray brighter, not blurrier.
                vColor.a *= size / (size + mbLen);
            }
        }
    }

    // Billboarding in WORLD space along those axes — rather than offsetting the
    // projected point — is what gives the sprite true perspective: one that drifts
    // toward the camera grows because it is nearer, not because a uniform said so.
    world += axisU * (corner.x * size * 0.5) + axisV * (corner.y * sizeV * 0.5);
    gl_Position = uVP * vec4(world, 1.0);
}

)KF"
    },
    { "vs_passthrough",
R"KF(attribute vec4 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
    gl_Position = aPosition;
    vTexCoord = aTexCoord;
}

)KF"
    },
    { "vs_quad",
R"KF(attribute vec2 aPos;
attribute vec2 aTex;
varying vec2 vTex;
void main() {
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
    vTex = aTex;
}

)KF"
    },
    { "vs_st",
R"KF(attribute vec4 aPos;
attribute vec4 aTex;
uniform mat4 uSt;
varying vec2 vTex;
void main() { gl_Position = aPos; vTex = (uSt * aTex).xy; }

)KF"
    },
    { "wipe_linear",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;    // 1/w, 1/h of this target
        uniform mat3 uLayerToLocal; // target uv -> the layer's own frame
        uniform float uCompletion;
        uniform float uAngle;
        uniform float uFeather;
        varying vec2 vTex;

        void main() {
            vec4 base = texture2D(uTexture, vTex);

vec3 q = uLayerToLocal * vec3(vTex, 1.0);
if (abs(q.z) < 0.001) {
    gl_FragColor = base;
    return;
}
float s = q.x / q.z;
float t = q.y / q.z;
vec2 row0 = vec2(uLayerToLocal[0][0], uLayerToLocal[1][0]);
vec2 row1 = vec2(uLayerToLocal[0][1], uLayerToLocal[1][1]);
vec2 row2 = vec2(uLayerToLocal[0][2], uLayerToLocal[1][2]);
// One screen pixel, in s and in t, at THIS fragment.
float pxS = length((row0 - s * row2) / q.z * uTexelSize);
float pxT = length((row1 - t * row2) / q.z * uTexelSize);
// The layer's size on screen, in pixels, at its centre (w == 1 there).
float layerW = 1.0 / max(length((row0 - 0.5 * row2) * uTexelSize), 1e-6);
float layerH = 1.0 / max(length((row1 - 0.5 * row2) * uTexelSize), 1e-6);
float layerAspect = layerW / layerH;
vec2 p = vec2(s * layerAspect, t);
float px = max(pxS * layerAspect, pxT);
// One composition pixel at 1080p (the convention Pixelate and Find Edges use), in
// pattern units: frame height over layer height, both as they stand on THIS target,
// so a "1 px" border is the same picture at preview resolution and in the export,
// and stays 1 px when the layer is scaled.
float pxUnit = 1.0 / (uTexelSize.y * layerH * 1080.0);

            // The direction the edge travels, in the layer's frame (y down).
            float ang = radians(uAngle);
            vec2 w = vec2(sin(ang), cos(ang));
            // Where this fragment sits along that direction, from the layer's
            // centre; `e` is how far the layer's corners reach each way along it.
            vec2 centre = vec2(0.5 * layerAspect, 0.5);
            float pos = dot(p - centre, w);
            float e = 0.5 * (layerAspect * abs(w.x) + abs(w.y));
            // Feather in 1080p pixels, never under one screen pixel.
            float f = max(uFeather * pxUnit, px);
            // From one feather before the first edge (nothing touched at 0) to the
            // last edge exactly (nothing left at 100).
            float edge = (-e - f) + uCompletion * (2.0 * e + f);
            float keep = smoothstep(edge, edge + f, pos);
            // Premultiplied in, premultiplied out.
            gl_FragColor = base * keep;
        }

)KF"
    },
    { "wipe_radial",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;    // 1/w, 1/h of this target
        uniform mat3 uLayerToLocal; // target uv -> the layer's own frame
        uniform float uCompletion;
        uniform float uStartAngle;
        uniform float uCenterX;
        uniform float uCenterY;
        uniform float uClockwise;
        uniform float uFeather;
        varying vec2 vTex;

        void main() {
            vec4 base = texture2D(uTexture, vTex);

vec3 q = uLayerToLocal * vec3(vTex, 1.0);
if (abs(q.z) < 0.001) {
    gl_FragColor = base;
    return;
}
float s = q.x / q.z;
float t = q.y / q.z;
vec2 row0 = vec2(uLayerToLocal[0][0], uLayerToLocal[1][0]);
vec2 row1 = vec2(uLayerToLocal[0][1], uLayerToLocal[1][1]);
vec2 row2 = vec2(uLayerToLocal[0][2], uLayerToLocal[1][2]);
// One screen pixel, in s and in t, at THIS fragment.
float pxS = length((row0 - s * row2) / q.z * uTexelSize);
float pxT = length((row1 - t * row2) / q.z * uTexelSize);
// The layer's size on screen, in pixels, at its centre (w == 1 there).
float layerW = 1.0 / max(length((row0 - 0.5 * row2) * uTexelSize), 1e-6);
float layerH = 1.0 / max(length((row1 - 0.5 * row2) * uTexelSize), 1e-6);
float layerAspect = layerW / layerH;
vec2 p = vec2(s * layerAspect, t);
float px = max(pxS * layerAspect, pxT);
// One composition pixel at 1080p (the convention Pixelate and Find Edges use), in
// pattern units: frame height over layer height, both as they stand on THIS target,
// so a "1 px" border is the same picture at preview resolution and in the export,
// and stays 1 px when the layer is scaled.
float pxUnit = 1.0 / (uTexelSize.y * layerH * 1080.0);

            vec2 c = p - vec2(uCenterX * layerAspect, uCenterY);
            float dist = length(c);
            // The very centre has no angle; it goes with the first half of the sweep.
            if (dist < 1e-5) {
                gl_FragColor = base * (1.0 - step(0.5, uCompletion));
                return;
            }
            // Clockwise from straight up (y points down in the layer's frame).
            float phi = atan(c.x, -c.y);
            float rel = phi - radians(uStartAngle);
            if (uClockwise < 0.5) rel = -rel;
            rel = mod(rel, 6.2831853);   // 0 .. 2π past the start line
            // Feather as an ANGLE at this radius: the same arc length everywhere.
            float f = max(uFeather * pxUnit, px) / max(dist, px);
            float edge = -f + uCompletion * (6.2831853 + f);
            float keep = smoothstep(edge, edge + f, rel);
            gl_FragColor = base * keep;
        }

)KF"
    },
    { "wipe_rect",
R"KF(#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;    // 1/w, 1/h of this target
        uniform mat3 uLayerToLocal; // target uv -> (s*w, t*w, w) in the LAYER's space
        uniform float uLeft;
        uniform float uTop;
        uniform float uRight;
        uniform float uBottom;
        varying vec2 vTex;

        void main() {
            // Undo the layer's own transform: (s, t) are 0..1 across and DOWN the
            // layer, however it is turned, scaled, mirrored or foreshortened. The map
            // is PROJECTIVE (a 3D layer's quad is a trapezoid under the camera), so it
            // carries a w to divide through — and a zero w is the renderer saying there
            // is no frame at all (a zero-size layer, an edge-on one), which has to hand
            // the pixels through rather than divide by nothing. The renderer normalises
            // w to 1 in the middle of the layer, so 0.001 means "no frame" on any layer
            // size — and stays representable in mediump, where 1e-6 is simply zero.
            vec3 q = uLayerToLocal * vec3(vTex, 1.0);
            if (abs(q.z) < 0.001) {
                gl_FragColor = texture2D(uTexture, vTex);
                return;
            }
            float s = q.x / q.z;
            float t = q.y / q.z;

            // Cropped past itself (Left + Right ≥ 100%): nothing survives. Without
            // the guard the two opposing ramps below would still overlap into a
            // half-transparent band sitting where the layer used to be.
            if (uLeft + uRight >= 1.0 || uTop + uBottom >= 1.0) {
                gl_FragColor = vec4(0.0);
                return;
            }

            // Half a pixel of ramp on each edge — the cut lands wherever the wheel
            // puts it, not on a texel boundary, so a hard step would show as a ragged
            // edge once the reduced-resolution preview is scaled back up.
            //
            // The width is the screen-space GRADIENT of s and t, so a layer that is
            // rotated, scaled or foreshortened still antialiases by one SCREEN pixel
            // rather than by one layer-space unit. mat3[c][r] is column c, row r, so
            // these pick the x/y parts of the three ROWS. (fwidth() would be the
            // obvious way and needs an extension GLES2 does not promise.)
            vec2 row0 = vec2(uLayerToLocal[0][0], uLayerToLocal[1][0]);
            vec2 row1 = vec2(uLayerToLocal[0][1], uLayerToLocal[1][1]);
            vec2 row2 = vec2(uLayerToLocal[0][2], uLayerToLocal[1][2]);
            float aaS = 0.5 * length((row0 - s * row2) / q.z * uTexelSize);
            float aaT = 0.5 * length((row1 - t * row2) / q.z * uTexelSize);
            float cov = smoothstep(uLeft - aaS, uLeft + aaS, s)
                      * (1.0 - smoothstep(1.0 - uRight - aaS, 1.0 - uRight + aaS, s))
                      * smoothstep(uTop - aaT, uTop + aaT, t)
                      * (1.0 - smoothstep(1.0 - uBottom - aaT, 1.0 - uBottom + aaT, t));

            // Premultiplied in, premultiplied out: scaling all four components by the
            // coverage is exactly "this pixel is that much less there".
            gl_FragColor = texture2D(uTexture, vTex) * cov;
        }

)KF"
    },
} };

// The vertex shader every full-screen effect pass uses. It is also in the
// table above; this reference is what the renderer actually binds.
constexpr std::string_view kPassthroughVertex = R"KF(
attribute vec2 aPos;
attribute vec2 aTex;
varying vec2 vTex;
void main() {
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
    vTex = aTex;
}
)KF";

} // namespace

std::string_view shaderSourceView(const std::string& stem)
{
    for (const Entry& entry : kShaders) {
        if (entry.stem == stem) return entry.source;
    }
    return {};
}

std::vector<std::string> shaderStems()
{
    std::vector<std::string> stems;
    stems.reserve(kShaders.size());
    for (const Entry& entry : kShaders) {
        stems.emplace_back(entry.stem);
    }
    return stems;
}

std::string shaderSource(const std::string& stem)
{
    return std::string(shaderSourceView(stem));
}

std::string passthroughVertexShader()
{
    return std::string(kPassthroughVertex);
}

} // namespace keyflow
