#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;   // 1/w, 1/h of the target
        uniform float uInvert;
        uniform float uWidth;
        uniform float uIntensity;
        uniform float uBlend;
        varying vec2 vTex;

        void main() {
            vec4 src = texture2D(uTexture, vTex);
            float a = src.a;

            float aspect = uTexelSize.y / uTexelSize.x;   // width / height
            // Frame-height units, x divided by the aspect so the kernel stays square
            // on a 16:9 comp — a horizontal edge and a vertical one of the same
            // contrast must come back the same weight.
            vec2 span = vec2(1.0 / aspect, 1.0) * (uWidth * (1.0 / 1080.0));
            // Never below one texel of THIS target. In a half-resolution preview the
            // frame-height distance lands inside a texel, and a sub-texel Sobel reads
            // the same bilinear sample twice and returns zero — the effect would
            // simply vanish while scrubbing and reappear in the export.
            vec2 d = max(span, uTexelSize);

            // Premultiplied, deliberately — see the doc comment. Eight taps; Sobel
            // gives the centre weight 0, so it is never fetched for the kernel.
            vec3 c00 = texture2D(uTexture, vTex + vec2(-d.x, -d.y)).rgb;
            vec3 c10 = texture2D(uTexture, vTex + vec2( 0.0, -d.y)).rgb;
            vec3 c20 = texture2D(uTexture, vTex + vec2( d.x, -d.y)).rgb;
            vec3 c01 = texture2D(uTexture, vTex + vec2(-d.x,  0.0)).rgb;
            vec3 c21 = texture2D(uTexture, vTex + vec2( d.x,  0.0)).rgb;
            vec3 c02 = texture2D(uTexture, vTex + vec2(-d.x,  d.y)).rgb;
            vec3 c12 = texture2D(uTexture, vTex + vec2( 0.0,  d.y)).rgb;
            vec3 c22 = texture2D(uTexture, vTex + vec2( d.x,  d.y)).rgb;

            // Sobel, per channel rather than on a luminance: that is what makes the
            // edges carry the colour of the transition.
            vec3 gx = (c20 + 2.0 * c21 + c22) - (c00 + 2.0 * c01 + c02);
            vec3 gy = (c02 + 2.0 * c12 + c22) - (c00 + 2.0 * c10 + c20);
            vec3 g = clamp(sqrt(gx * gx + gy * gy) * uIntensity, 0.0, 1.0);

            // AE: default is dark lines on white, Invert is bright lines on black.
            vec3 edge = mix(1.0 - g, g, step(0.5, uInvert));

            vec3 straight = src.rgb / max(a, 1e-4);
            vec3 outRgb = clamp(mix(edge, straight, uBlend), 0.0, 1.0);
            // Coverage is untouched, and the ground is multiplied by it: the drawing
            // lands inside the artwork's own silhouette, never across its box.
            gl_FragColor = vec4(outRgb * a, a);
        }
