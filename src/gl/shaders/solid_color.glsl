precision mediump float;
uniform sampler2D uTexture;
uniform float uColorR;
uniform float uColorG;
uniform float uColorB;
uniform float uOpacity;
varying vec2 vTex;
void main() {
    vec4 c = texture2D(uTexture, vTex);
    // The layer arrives premultiplied; mixing colours needs straight RGB,
    // otherwise a semi-transparent edge pulls the fill toward black.
    float a = c.a;
    vec3 rgb = c.rgb / max(a, 1e-4);
    rgb = mix(rgb, vec3(uColorR, uColorG, uColorB), uOpacity);
    gl_FragColor = vec4(rgb * a, a);
}
