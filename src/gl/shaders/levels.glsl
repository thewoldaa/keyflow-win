        precision mediump float;
        uniform sampler2D uTexture;
        uniform float uInBlack;
        uniform float uInWhite;
        uniform float uGamma;
        uniform float uOutBlack;
        uniform float uOutWhite;
        varying vec2 vTex;

        float remap(float c) {
            // The guard is not paranoia: dragging input white below input black is a
            // normal thing to do by accident, and an unguarded divide turns the whole
            // layer into NaN, which renders as black or as nothing depending on the
            // driver.
            float t = clamp((c - uInBlack) / max(uInWhite - uInBlack, 1e-4), 0.0, 1.0);
            t = pow(t, 1.0 / max(uGamma, 1e-3));
            return uOutBlack + t * (uOutWhite - uOutBlack);
        }

        void main() {

vec4 src = texture2D(uTexture, vTex);
float a = src.a;
vec3 rgb = clamp(src.rgb / max(a, 1e-4), 0.0, 1.0);

            vec3 v = clamp(vec3(remap(rgb.r), remap(rgb.g), remap(rgb.b)), 0.0, 1.0);
            gl_FragColor = vec4(v * a, a);
        }
