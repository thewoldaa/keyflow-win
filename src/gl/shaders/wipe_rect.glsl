#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform vec2 uTexelSize;    // 1/w, 1/h of this target
        uniform mat3 uLayerToLocal; // target uv -> (s*w, t*w, w) in the LAYER's space
        uniform float uLeft;
        uniform float uTop;
        uniform float uRight;
        uniform float uBottom;
        varying vec2 vTex;

        void main() {
            // Undo the layer's own transform: (s, t) are 0..1 across and DOWN the
            // layer, however it is turned, scaled, mirrored or foreshortened. The map
            // is PROJECTIVE (a 3D layer's quad is a trapezoid under the camera), so it
            // carries a w to divide through — and a zero w is the renderer saying there
            // is no frame at all (a zero-size layer, an edge-on one), which has to hand
            // the pixels through rather than divide by nothing. The renderer normalises
            // w to 1 in the middle of the layer, so 0.001 means "no frame" on any layer
            // size — and stays representable in mediump, where 1e-6 is simply zero.
            vec3 q = uLayerToLocal * vec3(vTex, 1.0);
            if (abs(q.z) < 0.001) {
                gl_FragColor = texture2D(uTexture, vTex);
                return;
            }
            float s = q.x / q.z;
            float t = q.y / q.z;

            // Cropped past itself (Left + Right ≥ 100%): nothing survives. Without
            // the guard the two opposing ramps below would still overlap into a
            // half-transparent band sitting where the layer used to be.
            if (uLeft + uRight >= 1.0 || uTop + uBottom >= 1.0) {
                gl_FragColor = vec4(0.0);
                return;
            }

            // Half a pixel of ramp on each edge — the cut lands wherever the wheel
            // puts it, not on a texel boundary, so a hard step would show as a ragged
            // edge once the reduced-resolution preview is scaled back up.
            //
            // The width is the screen-space GRADIENT of s and t, so a layer that is
            // rotated, scaled or foreshortened still antialiases by one SCREEN pixel
            // rather than by one layer-space unit. mat3[c][r] is column c, row r, so
            // these pick the x/y parts of the three ROWS. (fwidth() would be the
            // obvious way and needs an extension GLES2 does not promise.)
            vec2 row0 = vec2(uLayerToLocal[0][0], uLayerToLocal[1][0]);
            vec2 row1 = vec2(uLayerToLocal[0][1], uLayerToLocal[1][1]);
            vec2 row2 = vec2(uLayerToLocal[0][2], uLayerToLocal[1][2]);
            float aaS = 0.5 * length((row0 - s * row2) / q.z * uTexelSize);
            float aaT = 0.5 * length((row1 - t * row2) / q.z * uTexelSize);
            float cov = smoothstep(uLeft - aaS, uLeft + aaS, s)
                      * (1.0 - smoothstep(1.0 - uRight - aaS, 1.0 - uRight + aaS, s))
                      * smoothstep(uTop - aaT, uTop + aaT, t)
                      * (1.0 - smoothstep(1.0 - uBottom - aaT, 1.0 - uBottom + aaT, t));

            // Premultiplied in, premultiplied out: scaling all four components by the
            // coverage is exactly "this pixel is that much less there".
            gl_FragColor = texture2D(uTexture, vTex) * cov;
        }
