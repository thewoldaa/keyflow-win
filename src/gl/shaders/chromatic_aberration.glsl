precision mediump float;
uniform sampler2D uTexture;
uniform vec2 uTexelSize;   // 1/w, 1/h
uniform vec4 uLayerRect;
uniform float uSpread;
uniform float uShift;
uniform float uAngle;
varying vec2 vTex;
void main() {
    float aspect = uTexelSize.y / uTexelSize.x;   // width / height
    vec2 centre = (uLayerRect.xy + uLayerRect.zw) * 0.5;
    // Radial term: a scale about the centre, so it is already isotropic —
    // no aspect correction, unlike the flat shift below.
    vec2 radial = (vTex - centre) * (uSpread * 0.15);
    float ang = radians(uAngle);
    // Measured in frame HEIGHT units so the same number moves the same
    // distance whichever way the angle points.
    vec2 slide = vec2(cos(ang) / aspect, sin(ang)) * (uShift * 0.05);
    vec2 off = radial + slide;

    vec4 cr = texture2D(uTexture, vTex + off);
    vec4 cg = texture2D(uTexture, vTex);
    vec4 cb = texture2D(uTexture, vTex - off);
    // Each channel is unpremultiplied against ITS OWN sample's alpha: the
    // three taps land on pixels with different coverage, and dividing them
    // all by one alpha would pull the fringe toward black at every edge —
    // which is exactly the edge the effect exists to colour.
    vec3 straight = vec3(
        cr.r / max(cr.a, 1e-4),
        cg.g / max(cg.a, 1e-4),
        cb.b / max(cb.a, 1e-4)
    );
    // Coverage is the average of the three, so the silhouette splits too —
    // a real lens fringes the outline, not only the colour inside it.
    float a = (cr.a + cg.a + cb.a) / 3.0;
    gl_FragColor = vec4(clamp(straight, 0.0, 1.0) * a, a);
}
