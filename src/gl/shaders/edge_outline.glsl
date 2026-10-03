precision mediump float;
uniform sampler2D uTexture;   // original layer
uniform vec2 uTexelSize;
uniform float uDirection;
uniform float uDistance;
uniform float uSoftness;
varying vec2 vTex;
const int K = 8;
void main() {
    float aspect = uTexelSize.y / uTexelSize.x;
    float ang = radians(uDirection);
    vec2 off = vec2(cos(ang) / aspect, sin(ang)) * uDistance;
    float step = max(uSoftness, 1e-4) / float(K) / aspect;
    float sum = 0.0;
    float wsum = 0.0;
    for (int i = -K; i <= K; i++) {
        float w = exp(-3.0 * float(i * i) / float(K * K));
        sum += texture2D(uTexture, vTex - off + vec2(float(i) * step, 0.0)).a * w;
        wsum += w;
    }
    gl_FragColor = vec4(0.0, 0.0, 0.0, sum / wsum);
}
