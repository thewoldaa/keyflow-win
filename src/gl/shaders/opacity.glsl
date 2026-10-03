precision mediump float;
uniform sampler2D uTexture;
uniform float uAlpha;
varying vec2 vTex;
void main() {
    vec4 c = texture2D(uTexture, vTex);
    gl_FragColor = vec4(c.rgb * c.a * uAlpha, c.a * uAlpha);
}
