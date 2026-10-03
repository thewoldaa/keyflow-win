precision mediump float;
uniform sampler2D uTexture;     // the mask layer, rendered alone (premultiplied)
uniform sampler2D uBackdrop;    // what is already composited below it
uniform vec2 uTargetSize;
uniform int uMaskMode;
varying vec2 vTex;
void main() {
    vec4 dst = texture2D(uBackdrop, gl_FragCoord.xy / uTargetSize);
    vec4 m = texture2D(uTexture, vTex);
    float k = m.a;
    if (uMaskMode == 3 || uMaskMode == 4) {
        vec3 straight = m.rgb / max(m.a, 1e-4);
        k = dot(straight, vec3(0.299, 0.587, 0.114)) * m.a;
    }
    if (uMaskMode == 2 || uMaskMode == 4) k = 1.0 - k;
    gl_FragColor = dst * clamp(k, 0.0, 1.0);
}
