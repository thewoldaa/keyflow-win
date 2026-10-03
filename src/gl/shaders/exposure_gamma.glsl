        precision mediump float;
        uniform sampler2D uTexture;
        uniform float uExposure;
        uniform float uOffset;
        uniform float uGamma;
        varying vec2 vTex;
        void main() {

vec4 src = texture2D(uTexture, vTex);
float a = src.a;
vec3 rgb = clamp(src.rgb / max(a, 1e-4), 0.0, 1.0);

            vec3 v = rgb * pow(2.0, uExposure) + uOffset;
            // max() before pow: pow() of a negative base is undefined in GLSL, and
            // a negative offset takes dark pixels below zero as a matter of course.
            v = pow(max(v, vec3(0.0)), vec3(1.0 / max(uGamma, 1e-3)));
            v = clamp(v, 0.0, 1.0);
            gl_FragColor = vec4(v * a, a);
        }
