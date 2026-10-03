#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;
        uniform vec4 uLayerRect;
        uniform float uFractalType;
        uniform float uNoiseType;
        uniform float uInvert;
        uniform float uContrast;
        uniform float uBrightness;
        uniform float uOverflow;
        uniform float uRotation;
        uniform float uScale;
        uniform float uScaleWidth;
        uniform float uScaleHeight;
        uniform float uOffsetX;
        uniform float uOffsetY;
        uniform float uComplexity;
        uniform float uSubInfluence;
        uniform float uSubScaling;
        uniform float uSubRotation;
        uniform float uSubOffsetX;
        uniform float uSubOffsetY;
        uniform float uEvolution;
        uniform float uRandomSeed;
        uniform float uFitAlpha;
        uniform float uOpacity;
        varying vec2 vTex;


float insideLayer(vec2 uv) {
    vec2 lo = step(uLayerRect.xy, uv);
    vec2 hi = step(uv, uLayerRect.zw);
    return lo.x * lo.y * hi.x * hi.y;
}


float fitCoverage(float baseAlpha) {
    return mix(1.0, baseAlpha, step(0.5, uFitAlpha));
}


float fitInside(vec2 uv) {
    return mix(insideLayer(uv), 1.0, step(0.5, uFitAlpha));
}


        // No sin() in the hash: sin-based hashes lose their high frequencies on the
        // mobile GPUs that evaluate them at reduced precision, and the noise then
        // shows a visible grid. This one is pure fract/multiply.
        float hash(vec3 p) {
            p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
            p *= 17.0;
            return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
        }

        // Value noise; noiseType picks the curve between lattice points on x/y.
        // Linear shows the lattice as diamond-shaped creases, which is what AE's
        // Linear looks like too. The z (Evolution) axis is always smoothed: a Block
        // field should boil in place, not pop between cells.
        float valueNoise(vec3 x, int noiseType) {
            vec3 i = floor(x);
            vec3 f = fract(x);
            if (noiseType == 0) {
                f.xy = step(0.5, f.xy);                                  // Block
            } else if (noiseType == 2) {
                f.xy = f.xy * f.xy * (3.0 - 2.0 * f.xy);                 // Soft Linear
            } else if (noiseType == 3) {
                f.xy = f.xy * f.xy * f.xy * (f.xy * (f.xy * 6.0 - 15.0) + 10.0); // Spline
            }
            f.z = f.z * f.z * (3.0 - 2.0 * f.z);
            return mix(
                mix(mix(hash(i + vec3(0.0, 0.0, 0.0)), hash(i + vec3(1.0, 0.0, 0.0)), f.x),
                    mix(hash(i + vec3(0.0, 1.0, 0.0)), hash(i + vec3(1.0, 1.0, 0.0)), f.x), f.y),
                mix(mix(hash(i + vec3(0.0, 0.0, 1.0)), hash(i + vec3(1.0, 0.0, 1.0)), f.x),
                    mix(hash(i + vec3(0.0, 1.0, 1.0)), hash(i + vec3(1.0, 1.0, 1.0)), f.x), f.y),
                f.z
            );
        }

        vec2 rotate2(vec2 v, float degrees) {
            float c = cos(radians(degrees));
            float s = sin(radians(degrees));
            return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
        }

        // Identity through the middle, a rational shoulder over the last quarter at
        // each end that only reaches 0 and 1 at infinity — what Contrast pushed out
        // of range is compressed back rather than flattened into a plateau.
        float softClamp(float x) {
            float k = 0.25;
            float hi = x - (1.0 - k);
            if (hi > 0.0) x = (1.0 - k) + k * hi / (k + hi);
            float lo = k - x;
            if (lo > 0.0) x = k - k * lo / (k + lo);
            return x;
        }

        void main() {
            vec4 base = texture2D(uTexture, vTex);
            float amount = fitInside(vTex) * clamp(uOpacity, 0.0, 1.0);
            float cover = fitCoverage(base.a);
            // Bailing on zero coverage also skips the octave loop for every
            // transparent pixel — most of a text layer's box.
            if (amount <= 0.0 || cover <= 0.0) {
                gl_FragColor = base;
                return;
            }

            int fractalType = int(uFractalType + 0.5);
            int noiseType = int(uNoiseType + 0.5);
            int overflow = int(uOverflow + 0.5);

            float aspect = uTexelSize.y / uTexelSize.x;   // width / height
            // Height units, so a cell stays square and the field is the same size
            // in the reduced-resolution preview as in the export.
            vec2 offset = vec2(uOffsetX, uOffsetY);
            vec2 pos = rotate2(vec2(vTex.x * aspect, vTex.y) - offset, uRotation);
            // Where the frame's centre lands in that space: what Sub Rotation turns
            // the finer octaves about. Turning them about the origin instead would
            // pivot on the frame's top-left corner and sweep the far corner away.
            vec2 pivot = rotate2(vec2(0.5 * aspect, 0.5) - offset, uRotation);
            // Width/height stretch AFTER the rotation, i.e. along the noise's own
            // axes, so a stretched field turns as a whole when Rotation moves.
            vec2 stretch = vec2(max(uScaleWidth, 1e-3), max(uScaleHeight, 1e-3));
            // Cell size of the octave being sampled, in height units.
            float cell = max(uScale, 1e-3);
            // Whole seeds only, so a keyframed seed jumps between fields the way AE's
            // does instead of sliding the lattice through them.
            vec3 seedShift = floor(uRandomSeed + 0.5) * vec3(17.0, 29.0, 7.0);
            float z = uEvolution;

            float sum = 0.0;
            float norm = 0.0;
            float amp = 1.0;
            for (int i = 0; i < 8; i++) {
                // Fractional weight = the octave fades in instead of popping when
                // Complexity is keyframed.
                float w = clamp(uComplexity - float(i), 0.0, 1.0) * amp;
                // Uniform across the draw, so the branch really skips the octaves
                // above Complexity rather than sampling them for a weight of zero.
                if (w > 0.0) {
                    vec3 q = vec3(pos / (cell * stretch), z) + seedShift;
                    float v = valueNoise(q, noiseType);
                    float t = v;
                    if (fractalType == 1) {
                        t = abs(2.0 * v - 1.0);                 // Turbulent Basic
                    } else if (fractalType == 2) {
                        t = sqrt(abs(2.0 * v - 1.0));           // Turbulent Sharp
                    } else if (fractalType == 3) {
                        float a = 2.0 * v - 1.0;
                        t = a * a;                              // Turbulent Smooth
                    }
                    if (fractalType == 5) {
                        sum = max(sum, t * w);                  // Max
                        norm = 1.0;
                    } else {
                        sum += t * w;
                        norm += w;
                    }
                    if (fractalType == 4) {
                        // Dynamic: this octave's value pushes every finer one
                        // sideways, by up to half a cell each way. The second sample
                        // (a decorrelated slice of the same field) is the y push —
                        // one scalar would only ever smear along a diagonal.
                        float v2 = valueNoise(q + vec3(0.0, 0.0, 37.0), noiseType);
                        pos += (vec2(v, v2) - 0.5) * cell;
                    }
                }
                // Next octave: finer, turned and shifted by the Sub settings.
                amp *= uSubInfluence;
                cell /= max(uSubScaling, 1.0);
                z *= max(uSubScaling, 1.0);
                pos = rotate2(pos - pivot, uSubRotation) + pivot + vec2(uSubOffsetX, uSubOffsetY);
            }
            float n = sum / max(norm, 1e-4);

            // Contrast turns about mid-grey, so it opens the field out symmetrically
            // instead of also darkening it.
            n = (n - 0.5) * uContrast + 0.5 + uBrightness;
            if (overflow == 1) {
                n = softClamp(n);
            } else if (overflow == 2) {
                n = 1.0 - abs(mod(n, 2.0) - 1.0);   // Wrap Back: fold, don't flatten
            }
            n = clamp(n, 0.0, 1.0);
            n = mix(n, 1.0 - n, step(0.5, uInvert));

            // Premultiplied by the coverage it lands with — so on text the noise
            // fills the glyphs and keeps their edges, instead of their box.
            gl_FragColor = mix(base, vec4(vec3(n) * cover, cover), amount);
        }
