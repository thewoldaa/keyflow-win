precision mediump float;
uniform sampler2D uTexture;
uniform float uBrightness;
uniform float uContrast;
varying vec2 vTex;
void main() {
    vec4 c = texture2D(uTexture, vTex);
    // The layer arrives premultiplied; colour math needs straight RGB.
    float a = c.a;
    vec3 rgb = c.rgb / max(a, 1e-4);
    rgb += uBrightness;
    rgb = (rgb - 0.5) * (uContrast + 1.0) + 0.5;
    rgb = clamp(rgb, 0.0, 1.0);
    gl_FragColor = vec4(rgb * a, a);
}
