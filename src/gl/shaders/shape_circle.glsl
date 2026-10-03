#ifdef GL_FRAGMENT_PRECISION_HIGH
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
