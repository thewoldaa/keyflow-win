precision mediump float;
uniform sampler2D uTexture;   // pass-1 output (alpha in .a)
uniform sampler2D uInput;     // the stack's state as this effect received it
uniform vec2 uTexelSize;
uniform float uColorR;
uniform float uColorG;
uniform float uColorB;
uniform float uWidth;
varying vec2 vTex;
const int K = 8;
void main() {
    float step = uWidth / float(K);
    float m = 0.0;
    for (int i = -K; i <= K; i++) {
        m = max(m, texture2D(uTexture, vTex + vec2(0.0, float(i) * step)).a);
    }
    vec4 layer = texture2D(uInput, vTex);
    float border = clamp(m - layer.a, 0.0, 1.0);
    vec4 stroke = vec4(vec3(uColorR, uColorG, uColorB) * border, border);
    gl_FragColor = layer + stroke * (1.0 - layer.a);
}
