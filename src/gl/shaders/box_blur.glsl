#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D uTexture;
uniform vec2 uTexelSize;
uniform float uRadius;
uniform float uScaleX;
uniform float uScaleY;

uniform vec4 uLayerRect;
uniform float uRepeatEdge;
vec4 tap(vec2 uv, float hold) {
    vec2 held = clamp(uv, uLayerRect.xy, uLayerRect.zw);
    return texture2D(uTexture, mix(uv, held, hold));
}

varying vec2 vTex;
const int N = 12;
const float GOLDEN = 2.39996323;
void main() {
    float aspect = uTexelSize.y / uTexelSize.x;   // width / height
    float hold = step(0.5, uRepeatEdge);
    float R = uRadius * 0.12 * 0.16;
    vec2 stretch = vec2(uScaleX, uScaleY);
    vec4 sum = vec4(0.0);
    for (int i = 0; i < N; i++) {
        float phi = float(i) * GOLDEN;
        float r = sqrt((float(i) + 0.5) / float(N));
        vec2 o = vec2(cos(phi), sin(phi)) * (r * R) * stretch;
        sum += tap(vTex + vec2(o.x / aspect, o.y), hold);
    }
    vec2 d = min(vTex - uLayerRect.xy, uLayerRect.zw - vTex) / max(uTexelSize, vec2(1e-6));
    float inside = clamp(min(d.x, d.y) + 0.5, 0.0, 1.0);
    gl_FragColor = sum / float(N) * mix(1.0, inside, hold);
}
