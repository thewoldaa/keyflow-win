precision mediump float;
uniform sampler2D uTexture;
uniform vec2 uSrcTexelSize;
varying vec2 vTex;
void main() {
    vec2 o = uSrcTexelSize * 0.5;
    vec4 c = texture2D(uTexture, vTex + vec2(-o.x, -o.y))
           + texture2D(uTexture, vTex + vec2( o.x, -o.y))
           + texture2D(uTexture, vTex + vec2(-o.x,  o.y))
           + texture2D(uTexture, vTex + vec2( o.x,  o.y));
    gl_FragColor = c * 0.25;
}
