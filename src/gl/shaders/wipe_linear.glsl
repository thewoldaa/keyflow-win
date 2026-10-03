#ifdef GL_FRAGMENT_PRECISION_HIGH
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
