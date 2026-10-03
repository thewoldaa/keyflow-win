precision mediump float;
uniform sampler2D uTexture;   // pass-1 output (alpha in .a)
uniform sampler2D uInput;     // the stack's state as this effect received it
uniform vec2 uTexelSize;
uniform float uShadowR;
uniform float uShadowG;
uniform float uShadowB;
uniform float uOpacity;
uniform float uSoftness;
varying vec2 vTex;
const int K = 8;
void main() {
    float step = max(uSoftness, 1e-4) / float(K);
    float sum = 0.0;
    float wsum = 0.0;
    for (int i = -K; i <= K; i++) {
        float w = exp(-3.0 * float(i * i) / float(K * K));
        sum += texture2D(uTexture, vTex + vec2(0.0, float(i) * step)).a * w;
        wsum += w;
    }
    float shA = (sum / wsum) * clamp(uOpacity, 0.0, 1.0);
    vec4 shadow = vec4(vec3(uShadowR, uShadowG, uShadowB) * shA, shA);
    vec4 layer = texture2D(uInput, vTex);   // premultiplied
    gl_FragColor = layer + shadow * (1.0 - layer.a);
}
