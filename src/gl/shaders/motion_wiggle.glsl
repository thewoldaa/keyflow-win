#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;    // 1/w, 1/h
        uniform float uTime;        // seconds since the layer's in point
        uniform float uSpeed;       // cycles per second
        uniform float uStrength;    // travel, fraction of frame height
        uniform float uAngle;       // degrees: direction of travel
        uniform float uPhase;       // degrees: where in the cycle t = 0 sits
        uniform float uDecay;       // 1/seconds; 0 = never settles
        varying vec2 vTex;


float turns(float t, float speed, float phaseDeg) {
    return fract(t * speed + phaseDeg / 360.0);
}


float falloff(float t, float decay) {
    return exp(-decay * t);
}


vec4 sampleOrNothing(sampler2D tex, vec2 uv) {
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return vec4(0.0);
    return texture2D(tex, uv);
}


uniform sampler2D uWideSource;
uniform float uWideCopy;
uniform vec4 uWideFit;        // (kx, ky, ox, oy): layer (s, t) -> copy (s*kx+ox, t*ky+oy)
uniform mat3 uLayerToLocal;   // target uv -> (s*w, t*w, w) in the LAYER's space
uniform mat3 uPreMotion;      // layer point -> the layer point drawn there by the passes above
uniform float uPreMotionOn;
uniform float uFoldFade;      // 1 - opacity of the Transforms folded into uPreMotion

vec4 sampleLayer(vec2 uv) {
    if (uWideCopy < 0.5) return sampleOrNothing(uTexture, uv);
    vec3 q = uLayerToLocal * vec3(uv, 1.0);
    // At or past the horizon of a tilted plane there is no layer point (a degenerate
    // matrix never reaches here: the renderer draws no copy for a layer it cannot lay
    // flat).
    if (q.z < 1e-4) return vec4(0.0);
    vec2 p = q.xy / q.z;
    if (uPreMotionOn > 0.5) {
        vec3 m = uPreMotion * vec3(p, 1.0);
        if (m.z <= 0.0) return vec4(0.0);
        p = m.xy / m.z;
    }
    vec4 fit = uWideFit.x > 0.0 ? uWideFit : vec4(1.0, 1.0, 0.0, 0.0);
    // t runs DOWN the layer, the texture's v runs up.
    vec2 c = vec2(p.x * fit.x + fit.z, 1.0 - (p.y * fit.y + fit.w));
    if (c.x < 0.0 || c.x > 1.0 || c.y < 0.0 || c.y > 1.0) return vec4(0.0);
    return texture2D(uWideSource, c) * (1.0 - uFoldFade);
}


        void main() {
            float aspect = uTexelSize.y / uTexelSize.x;   // width / height
            // A layer can be sampled before its own in point (motion blur reaches
            // back across the shutter); the clock must not run backwards there.
            float t = max(uTime, 0.0);
            float wave = sin(6.2831853 * turns(t, uSpeed, uPhase));
            float amp = uStrength * falloff(t, uDecay);
            float ang = radians(uAngle);
            // x divided by the aspect: the offset is measured in frame HEIGHTS, so
            // the same Strength travels the same distance whichever way it points.
            vec2 off = vec2(cos(ang) / aspect, sin(ang)) * (wave * amp);
            // MINUS: moving the picture one way means reading from the other.
            gl_FragColor = sampleLayer(vTex - off);
        }
