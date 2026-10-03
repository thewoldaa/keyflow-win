#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D uTexture;
uniform vec2 uTexelSize;
uniform float uRadius;
uniform float uScaleX;
uniform float uScaleY;
uniform float uIrisShape;          // index into the shape list — see the spec
uniform float uIrisCurvature;      // + toward a circle, - toward a star
uniform float uRotation;           // degrees
uniform float uBokeh;              // 1 = flat disc, 0 = Gaussian-weighted
uniform float uBokehShading;       // + rim, - centre

uniform vec4 uLayerRect;
uniform float uRepeatEdge;
vec4 tap(vec2 uv, float hold) {
    vec2 held = clamp(uv, uLayerRect.xy, uLayerRect.zw);
    return texture2D(uTexture, mix(uv, held, hold));
}

varying vec2 vTex;
const int N = 64;
const float GOLDEN = 2.39996323;

// How far the aperture reaches in direction `phi`, 0..1: the polygon's edge (a
// vertex at 1), bent by the curvature; 1 everywhere for a circle.
float iris(float phi, float sides, float curve) {
    if (sides < 2.5) return 1.0;
    float sector = 6.28318530 / sides;
    float a = mod(phi, sector) - sector * 0.5;
    float poly = min(cos(sector * 0.5) / max(cos(a), 1e-3), 1.0);
    // Positive: the blades bow out toward the circle. Negative: the edge midpoints
    // are pulled in toward the centre, a star (poly is at least cos(sector/2) > 0,
    // so the pow has a safe base).
    return curve >= 0.0 ? mix(poly, 1.0, curve) : pow(poly, 1.0 + 4.0 * (-curve));
}

void main() {
    float aspect = uTexelSize.y / uTexelSize.x;   // width / height
    float hold = step(0.5, uRepeatEdge);
    float R = uRadius * 0.12;         // frame-height units
    // Circle, then 3 … 16 sides.
    int shape = int(uIrisShape + 0.5);
    float sides = shape == 0 ? 0.0 : float(shape + 2);
    float rot = radians(uRotation);
    vec2 stretch = vec2(uScaleX, uScaleY);
    vec4 sum = vec4(0.0);
    float wsum = 0.0;
    for (int i = 0; i < N; i++) {
        float phi = float(i) * GOLDEN;
        float r0 = sqrt((float(i) + 0.5) / float(N));
        float r = r0 * iris(phi + rot, sides, uIrisCurvature);
        vec2 o = vec2(cos(phi), sin(phi)) * (r * R) * stretch;
        vec4 s = tap(vTex + vec2(o.x / aspect, o.y), hold);
        float w = mix(exp(-2.5 * r0 * r0), 1.0, uBokeh);
        w *= max(1.0 + uBokehShading * (2.0 * r0 * r0 - 1.0), 0.0);
        sum += s * w;
        wsum += w;
    }
    gl_FragColor = sum / max(wsum, 1e-4);
}
