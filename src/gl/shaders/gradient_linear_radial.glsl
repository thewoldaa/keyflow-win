#ifdef GL_FRAGMENT_PRECISION_HIGH
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
