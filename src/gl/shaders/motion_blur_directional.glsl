#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
uniform sampler2D uTexture;
uniform mat3 uMbStart;
uniform mat3 uMbEnd;
uniform vec2 uTargetSize;
uniform float uMaxVelPx;
uniform int uSamples;
varying vec2 vTex;
void main() {
    vec3 p = vec3(vTex, 1.0);
    vec3 s = uMbStart * p;
    vec3 e = uMbEnd * p;
    vec2 vel = vec2(0.0);
    // Both matrices are normalised to w = 1 at the middle of the layer, so a w this
    // small is a point on the mapping's horizon (or a dead all-zero matrix): there is
    // no honest velocity for it, and a sharp pixel beats a wild streak.
    if (abs(s.z) > 1e-4 && abs(e.z) > 1e-4) vel = e.xy / e.z - s.xy / s.z;
    float travel = length(vel * uTargetSize);
    if (travel > uMaxVelPx) vel *= uMaxVelPx / travel;
    // Interleaved-gradient noise in [0,1): the inner fract keeps every intermediate
    // small, so this stays well-conditioned even in mediump.
    float d = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    vec4 acc = vec4(0.0);
    float n = float(uSamples);
    for (int i = 0; i < 48; i++) {
        if (i >= uSamples) break;
        float t = (float(i) + d) / n - 0.5;
        acc += texture2D(uTexture, vTex + vel * t);
    }
    gl_FragColor = acc / n;
}
