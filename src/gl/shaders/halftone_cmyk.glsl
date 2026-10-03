#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;   // 1/w, 1/h of the target
        uniform float uCellSize;
        uniform float uAngle;
        uniform float uCmyk;
        uniform float uGain;
        uniform float uSoftness;
        uniform float uInkR;
        uniform float uInkG;
        uniform float uInkB;
        uniform float uPaperEnabled;
        uniform float uPaperR;
        uniform float uPaperG;
        uniform float uPaperB;
        varying vec2 vTex;

        vec2 turn(vec2 v, float a) {
            float c = cos(a);
            float s = sin(a);
            return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
        }

        // One screen: the average (premultiplied) colour of the cell this fragment
        // falls in on a grid turned by `ang`, and through `dist` how far the fragment
        // sits from that cell's centre, in cell units. Four stratified taps — a point
        // sample of detailed footage picks a different texel every time the picture
        // moves a hair, and the dots would flicker.
        vec4 screenCell(vec2 p, float cell, float ang, vec2 toUnits, out float dist) {
            vec2 r = turn(p, ang) / cell;
            vec2 idx = floor(r);
            dist = length(fract(r) - 0.5);
            vec2 centreUv = turn((idx + 0.5) * cell, -ang) / toUnits + 0.5;
            vec4 sum = vec4(0.0);
            for (int j = 0; j < 2; j++) {
                for (int i = 0; i < 2; i++) {
                    vec2 o = (vec2(float(i), float(j)) - 0.5) * 0.5;   // ±0.25 of the cell
                    sum += texture2D(uTexture, centreUv + turn(o * cell, -ang) / toUnits);
                }
            }
            return sum * 0.25;
        }

        vec3 straight(vec4 c) {
            return clamp(c.rgb / max(c.a, 1e-4), 0.0, 1.0);
        }

        // RGB → CMYK with under-colour removal: the grey the three inks share
        // moves onto the black plate.
        vec4 toCmyk(vec3 s) {
            vec3 cmy = 1.0 - s;
            float k = min(cmy.r, min(cmy.g, cmy.b));
            return vec4((cmy - k) / max(1.0 - k, 1e-4), k);
        }

        // How much of this fragment a dot of the given tone covers.
        float dotCover(float darkness, float dist, float aa) {
            // Dot Gain as a gamma on the tone. pow(0, y) is undefined in GLSL ES,
            // and its NaN survives every guard after it — floor the base.
            float k = pow(max(clamp(darkness, 0.0, 1.0), 1e-4), 1.0 / max(uGain, 1e-2));
            // Area ∝ tone: r = sqrt(k / π) covers exactly k of the cell — up to the
            // deep shadows, then a shoulder past the half-diagonal (0.707) so black
            // prints solid instead of leaving a paper speck in every corner.
            float r = 0.5642 * sqrt(k) + smoothstep(0.85, 1.0, k) * (0.78 - 0.5642);
            // The band sits INSIDE the radius — centred on it, a zero-radius dot
            // would still half-light its centre texel and white would be freckled.
            return 1.0 - smoothstep(r - 2.0 * aa, r, dist);
        }

        void main() {
            vec4 src = texture2D(uTexture, vTex);
            float a = src.a;


            float aspect = uTexelSize.y / uTexelSize.x;   // width / height
            vec2 toUnits = vec2(aspect, 1.0);
            vec2 p = (vTex - 0.5) * toUnits;
            float cell = max(uCellSize * (1.0 / 1080.0), uTexelSize.y);

            float base = radians(uAngle);
            // Half a texel each side in cell units, widened by Softness toward a
            // blurred tone.
            float aa = 0.5 * uTexelSize.y / cell + clamp(uSoftness, 0.0, 1.0) * 0.3;

            vec3 trans;   // what the ink lets through, per channel: 1 = bare paper
            float cov;    // how much of this fragment is inked at all
            if (uCmyk > 0.5) {
                // The press angles — C 15°, M 75°, Y 0°, K 45° — relative to Angle,
                // which is where the black screen sits. Each ink reads its OWN cell.
                float dC; float dM; float dY; float dK;
                vec3 sC = straight(screenCell(p, cell, base - radians(30.0), toUnits, dC));
                vec3 sM = straight(screenCell(p, cell, base + radians(30.0), toUnits, dM));
                vec3 sY = straight(screenCell(p, cell, base - radians(45.0), toUnits, dY));
                vec3 sK = straight(screenCell(p, cell, base, toUnits, dK));
                float c = dotCover(toCmyk(sC).x, dC, aa);
                float m = dotCover(toCmyk(sM).y, dM, aa);
                float y = dotCover(toCmyk(sY).z, dY, aa);
                float k = dotCover(toCmyk(sK).w, dK, aa);
                // Process inks as filters: cyan takes the red out, magenta the green,
                // yellow the blue, black everything. The product is the overprint.
                trans = (1.0 - c * vec3(1.0, 0.0, 0.0))
                      * (1.0 - m * vec3(0.0, 1.0, 0.0))
                      * (1.0 - y * vec3(0.0, 0.0, 1.0))
                      * (1.0 - k);
                cov = 1.0 - (1.0 - c) * (1.0 - m) * (1.0 - y) * (1.0 - k);
            } else {
                float d;
                vec3 s = straight(screenCell(p, cell, base, toUnits, d));
                float dark = 1.0 - dot(s, vec3(0.2126, 0.7152, 0.0722));
                cov = dotCover(dark, d, aa);
                trans = mix(vec3(1.0), vec3(uInkR, uInkG, uInkB), cov);
            }

            // The ink's own colour, straight: `trans` with the bare paper taken back
            // out of it (over white, trans = (1 - cov) + inkColour * cov).
            vec3 inkColour = cov > 1e-4 ? clamp((trans - (1.0 - cov)) / cov, 0.0, 1.0) : vec3(0.0);
            if (uPaperEnabled > 0.5) {
                // Printed ON the paper: paper where there is no ink, ink where there
                // is. Gated by the layer's own alpha, so the print lands inside the
                // artwork's silhouette and never across its box.
                vec3 rgb = mix(vec3(uPaperR, uPaperG, uPaperB), inkColour, cov);
                gl_FragColor = vec4(rgb * a, a);
            } else {
                // Just the dots, over whatever is underneath.
                gl_FragColor = vec4(inkColour * cov, cov) * a;
            }
        }
