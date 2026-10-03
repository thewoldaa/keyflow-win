precision mediump float;
uniform sampler2D uTexture;   // original layer
uniform vec2 uTexelSize;
uniform float uWidth;
varying vec2 vTex;
const int K = 8;
void main() {
    float aspect = uTexelSize.y / uTexelSize.x;
    float step = uWidth / float(K) / aspect;
    float m = 0.0;
    for (int i = -K; i <= K; i++) {
        m = max(m, texture2D(uTexture, vTex + vec2(float(i) * step, 0.0)).a);
    }
    gl_FragColor = vec4(0.0, 0.0, 0.0, m);
}
