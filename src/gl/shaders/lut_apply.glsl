        precision mediump float;
        uniform sampler2D uTexture;
        // 256 x 1: texel i is what level i/255 becomes, per channel, Master folded in.
        uniform sampler2D uLut;
        varying vec2 vTex;

        void main() {

vec4 src = texture2D(uTexture, vTex);
float a = src.a;
vec3 rgb = clamp(src.rgb / max(a, 1e-4), 0.0, 1.0);

            // Level 0 has to land on the CENTRE of texel 0 and level 1 on the centre
            // of texel 255, or the linear filter would blend the ends with the clamp
            // and the table's black and white would both be off by half a step.
            vec3 t = rgb * (255.0 / 256.0) + 0.5 / 256.0;
            vec3 v = vec3(
                texture2D(uLut, vec2(t.r, 0.5)).r,
                texture2D(uLut, vec2(t.g, 0.5)).g,
                texture2D(uLut, vec2(t.b, 0.5)).b
            );
            gl_FragColor = vec4(v * a, a);
        }
