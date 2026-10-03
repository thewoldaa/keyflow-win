#ifdef GL_FRAGMENT_PRECISION_HIGH
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
