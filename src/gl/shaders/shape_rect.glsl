#ifdef GL_FRAGMENT_PRECISION_HIGH
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
