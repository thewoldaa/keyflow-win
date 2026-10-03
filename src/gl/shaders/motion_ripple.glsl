#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;    // 1/w, 1/h
        uniform vec4 uLayerRect;    // the layer's box, in 0..1 target space
        uniform float uTime;        // seconds since the layer's in point
        uniform float uFrequency;   // jolts per second
        uniform float uStrength;    // travel, fraction of frame height
        uniform float uRotation;    // degrees, peak
        uniform float uScale;       // peak zoom, as a fraction (0.02 = +/-2%)
        uniform float uSoften;      // 0 = held steps, 1 = eased between them
        uniform float uDecay;       // 1/seconds; 0 = never settles
        uniform float uSeed;
        varying vec2 vTex;


float falloff(float t, float decay) {
    return exp(-decay * t);
}


vec2 unrotate(vec2 p, float deg, float aspect) {
    p.x *= aspect;
    float a = -radians(deg);
    float c = cos(a);
    float s = sin(a);
    p = mat2(c, -s, s, c) * p;
    p.x /= aspect;
    return p;
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


        // Hash without sine: sin-based hashes differ between GPU vendors, and a
        // shake that lands on different values on the export device than in the
        // preview is not the same shot.
        float hash11(float p) {
            p = fract(p * 0.1031);
            p *= p + 33.33;
            p *= p + p;
            return fract(p);
        }

        // One value per step of one random stream, in -1..1. Held flat across the
        // step; uSoften eases it into the next one instead.
        float jolt(float stream, float tick, float f) {
            float a = hash11(tick + stream);
            float b = hash11(tick + 1.0 + stream);
            float k = smoothstep(0.0, 1.0, f) * clamp(uSoften, 0.0, 1.0);
            return mix(a, b, k) * 2.0 - 1.0;
        }

        void main() {
            float aspect = uTexelSize.y / uTexelSize.x;
            float t = max(uTime, 0.0);
            float ticks = t * uFrequency;
            float tick = floor(ticks);
            float f = ticks - tick;
            // Streams far enough apart that no two ever walk the same sequence.
            float seed = uSeed * 37.0;
            float fall = falloff(t, uDecay);

            vec2 off = vec2(
                jolt(seed, tick, f) / aspect,
                jolt(seed + 11.0, tick, f)
            ) * (uStrength * fall);
            float rot = jolt(seed + 23.0, tick, f) * uRotation * fall;
            float scale = 1.0 + jolt(seed + 41.0, tick, f) * uScale * fall;

            // Turns and zooms about the layer's own centre — a shake pivoting on the
            // frame centre would fling an off-centre layer across the screen.
            vec2 pivot = (uLayerRect.xy + uLayerRect.zw) * 0.5;
            vec2 p = vTex - pivot - off;
            p = unrotate(p, rot, aspect);
            p /= max(scale, 1e-4);
            gl_FragColor = sampleLayer(pivot + p);
        }
