#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;   // 1/w, 1/h of the target
        uniform vec4 uLayerRect;   // the layer's box in target uv, for the vignette
        uniform float uCellSize;
        uniform float uGap;
        uniform float uRoundCells;
        uniform float uShade;
        uniform float uVignette;
        uniform float uBackEnabled;
        uniform float uBackR;
        uniform float uBackG;
        uniform float uBackB;
        varying vec2 vTex;

        void main() {

            float aspect = uTexelSize.y / uTexelSize.x;   // width / height
            vec2 toUnits = vec2(aspect, 1.0);
            vec2 p = (vTex - 0.5) * toUnits;
            float cell = max(uCellSize * (1.0 / 1080.0), uTexelSize.y);

            vec2 g = p / cell;
            vec2 idx = floor(g);
            // This fragment inside its cell, -0.5..0.5 each way.
            vec2 c = fract(g) - 0.5;

            // The cell's colour is its AVERAGE — sixteen stratified taps, a box
            // filter that holds still while the footage moves. See the doc comment.
            vec2 centreUv = (idx + 0.5) * cell / toUnits + 0.5;
            vec2 cellUv = vec2(cell) / toUnits;
            vec4 sum = vec4(0.0);
            for (int j = 0; j < 4; j++) {
                for (int i = 0; i < 4; i++) {
                    vec2 o = (vec2(float(i), float(j)) + 0.5) / 4.0 - 0.5;
                    sum += texture2D(uTexture, centreUv + o * cellUv);
                }
            }
            // Premultiplied in, premultiplied out: averaging premultiplied colour IS
            // the box filter, and a half-covered cell comes out half transparent.
            vec4 avg = sum / 16.0;

            // The lit part of the cell: a square or a disc, inset by the gap.
            float inner = 0.5 * (1.0 - clamp(uGap, 0.0, 0.95));
            float dist = mix(max(abs(c.x), abs(c.y)), length(c), step(0.5, uRoundCells));
            // Half a texel of antialiasing each side, in cell units.
            float aa = 0.5 * uTexelSize.y / cell;
            float shape = 1.0 - smoothstep(inner - aa, inner + aa, dist);
            // The LED's falloff: brightest in the middle, Cell Shade darker at the rim.
            float t = clamp(dist / max(inner, 1e-4), 0.0, 1.0);
            float lit = shape * (1.0 - clamp(uShade, 0.0, 1.0) * t * t);

            vec4 outc;
            if (uBackEnabled > 0.5) {
                // The panel behind the LEDs: gaps and rim show the backing colour.
                // Scaled by the cell's coverage, so it is painted inside the object's
                // own (blocky) silhouette and never across the layer's box.
                vec3 back = vec3(uBackR, uBackG, uBackB) * avg.a;
                outc = vec4(mix(back, avg.rgb, lit), avg.a);
            } else {
                // No panel: transparent gaps, a rim that fades out. Scaling all four
                // components keeps the result premultiplied.
                outc = avg * lit;
            }

            // Vignette over the layer's box — the Vignette effect's maths, under its
            // rule: RGB only, never alpha, or the layer underneath would show through
            // the darkened corners instead of the picture going dark.
            vec2 vc = (uLayerRect.xy + uLayerRect.zw) * 0.5;
            vec2 halfBox = max((uLayerRect.zw - uLayerRect.xy) * 0.5, vec2(1e-4));
            float d = length((vTex - vc) / halfBox);
            float vig = 1.0 - clamp(uVignette, 0.0, 1.0) * smoothstep(0.5, 1.35, d);
            gl_FragColor = vec4(outc.rgb * vig, outc.a);
        }
