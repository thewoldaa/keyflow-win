#ifdef GL_FRAGMENT_PRECISION_HIGH
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
