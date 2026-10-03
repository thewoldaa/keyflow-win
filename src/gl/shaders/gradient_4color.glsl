#ifdef GL_FRAGMENT_PRECISION_HIGH
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
