precision mediump float;
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
