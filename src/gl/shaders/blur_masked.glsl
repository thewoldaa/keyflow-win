precision mediump float;
uniform sampler2D uTexture;   // the blurred quarter-size result
uniform sampler2D uInput;     // the stack's state as this effect received it
uniform vec2 uSrcTexelSize;
uniform float uRadius;
varying vec2 vTex;
void main() {
    vec2 t = uSrcTexelSize;
    // 3x3 tent, like the glow's upsample: plain bilinear from a quarter-size source
    // leaves square banding that is obvious across a smooth gradient.
    vec4 s = texture2D(uTexture, vTex + vec2(-t.x,  t.y))
           + texture2D(uTexture, vTex + vec2( 0.0,  t.y)) * 2.0
           + texture2D(uTexture, vTex + vec2( t.x,  t.y))
           + texture2D(uTexture, vTex + vec2(-t.x,  0.0)) * 2.0
           + texture2D(uTexture, vTex)                    * 4.0
           + texture2D(uTexture, vTex + vec2( t.x,  0.0)) * 2.0
           + texture2D(uTexture, vTex + vec2(-t.x, -t.y))
           + texture2D(uTexture, vTex + vec2( 0.0, -t.y)) * 2.0
           + texture2D(uTexture, vTex + vec2( t.x, -t.y));
    s *= 0.0625;
    float k = clamp(uRadius / 0.04, 0.0, 1.0);
    gl_FragColor = mix(texture2D(uInput, vTex), s, k);
}
