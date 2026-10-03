#ifdef GL_FRAGMENT_PRECISION_HIGH
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
                // reaches into the neighbours', so the three nearest are tried.
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
