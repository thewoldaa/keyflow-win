#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;   // pass-1 output: blurred alpha in .a
        uniform sampler2D uInput;     // the layer as this effect received it
        uniform vec2 uTexelSize;
        uniform vec4 uLayerRect;
        uniform float uCenterX;
        uniform float uCenterY;
        uniform float uDirection;
        uniform float uShape;
        uniform float uWidth;
        uniform float uSweepIntensity;
        uniform float uEdgeIntensity;
        uniform float uEdgeThickness;
        uniform float uColorR;
        uniform float uColorG;
        uniform float uColorB;
        uniform float uLightReception;
        varying vec2 vTex;
        const int K = 8;

        void main() {
            vec4 base = texture2D(uInput, vTex);

            float step = max(uEdgeThickness, 1e-4) / float(K);
            float sum = 0.0;
            float wsum = 0.0;
            for (int i = -K; i <= K; i++) {
                float w = exp(-3.0 * float(i * i) / float(K * K));
                sum += texture2D(uTexture, vTex + vec2(0.0, float(i) * step)).a * w;
                wsum += w;
            }
            float soft = sum / wsum;
            // Rim: covered here, but the coverage blurred over Edge Thickness is not —
            // so the alpha edge is within that distance. A pixel right on the edge
            // blurs to one half, hence the doubling: the edge itself is a full rim.
            float rim = clamp(base.a * (1.0 - soft) * 2.0, 0.0, 1.0);

            float aspect = uTexelSize.y / uTexelSize.x;
            vec2 size = max(uLayerRect.zw - uLayerRect.xy, vec2(1e-4));
            vec2 centre = uLayerRect.xy + vec2(uCenterX, uCenterY) * size;
            // AE's angle: 0 stands the band upright and it turns clockwise on
            // screen. uv's y points up, so the band's normal at angle a is
            // (cos a, -sin a). Distance in frame-height units, so Width means the
            // same thickness at any angle.
            float ang = radians(uDirection);
            vec2 n = vec2(cos(ang), -sin(ang));
            float x = abs(dot((vTex - centre) * vec2(aspect, 1.0), n));
            float w = max(uWidth, 1e-4);
            float t = clamp(1.0 - x / w, 0.0, 1.0);   // 1 on the centre line, 0 at the edge

            int shape = int(uShape + 0.5);
            float body;
            if (shape == 0) {
                body = t;                                 // Linear
            } else if (shape == 1) {
                body = 0.5 - 0.5 * cos(t * 3.14159265);   // Smooth
            } else {
                body = t * t * t;                         // Sharp: gathered at the centre
            }

            vec3 light = vec3(uColorR, uColorG, uColorB);
            // The rim only flares where the band is: the light catches the edge as
            // it passes, it does not outline the whole layer.
            float amount = body * (uSweepIntensity + rim * uEdgeIntensity);
            int reception = int(uLightReception + 0.5);
            if (reception == 2) {
                // Cutout: the sweep alone, cut to the layer's shape.
                float cov = clamp(amount, 0.0, 1.0) * base.a;
                gl_FragColor = vec4(light * cov, cov);
            } else if (reception == 1) {
                // Composite: the light laid over the pixel, so a strong sweep goes
                // to the light's colour rather than to white.
                float k = clamp(amount, 0.0, 1.0);
                gl_FragColor = vec4(mix(base.rgb, light * base.a, k), base.a);
            } else {
                // Add. Times the layer's own coverage: the light belongs to the
                // artwork, not to the empty space around it. Alpha is untouched, so
                // the result has to be clamped to it to stay a valid premultiplied
                // colour.
                vec3 lit = base.rgb + light * (amount * base.a);
                gl_FragColor = vec4(min(lit, vec3(base.a)), base.a);
            }
        }
