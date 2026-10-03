precision mediump float;
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
