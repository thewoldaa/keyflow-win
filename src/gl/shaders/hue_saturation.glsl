        precision mediump float;
        uniform sampler2D uTexture;
        uniform float uHue;
        uniform float uLightness;
        uniform float uSaturation;
        varying vec2 vTex;

        vec3 toHsl(vec3 c) {
            float mx = max(c.r, max(c.g, c.b));
            float mn = min(c.r, min(c.g, c.b));
            float l = (mx + mn) * 0.5;
            float d = mx - mn;
            float h = 0.0;
            float s = 0.0;
            if (d > 1e-5) {
                s = (l > 0.5) ? d / max(2.0 - mx - mn, 1e-5) : d / max(mx + mn, 1e-5);
                if (mx == c.r) {
                    h = (c.g - c.b) / d + ((c.g < c.b) ? 6.0 : 0.0);
                } else if (mx == c.g) {
                    h = (c.b - c.r) / d + 2.0;
                } else {
                    h = (c.r - c.g) / d + 4.0;
                }
                h /= 6.0;
            }
            return vec3(h, s, l);
        }

        float channel(float p, float q, float t) {
            t = fract(t);
            if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
            if (t < 0.5) return q;
            if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
            return p;
        }

        vec3 toRgb(vec3 hsl) {
            // Grey has no hue to reconstruct; the general path would divide by a
            // saturation of zero and speckle every neutral pixel.
            if (hsl.y < 1e-5) return vec3(hsl.z);
            float q = (hsl.z < 0.5)
                ? hsl.z * (1.0 + hsl.y)
                : hsl.z + hsl.y - hsl.z * hsl.y;
            float p = 2.0 * hsl.z - q;
            return vec3(
                channel(p, q, hsl.x + 1.0 / 3.0),
                channel(p, q, hsl.x),
                channel(p, q, hsl.x - 1.0 / 3.0)
            );
        }

        void main() {

vec4 src = texture2D(uTexture, vTex);
float a = src.a;
vec3 rgb = clamp(src.rgb / max(a, 1e-4), 0.0, 1.0);

            vec3 hsl = toHsl(rgb);
            // Hue wraps: -1..1 is a full turn either way, and fract() in `channel`
            // takes care of the wrap-around at red.
            hsl.x = hsl.x + uHue * 0.5;
            hsl.z = (uLightness >= 0.0)
                ? mix(hsl.z, 1.0, uLightness)
                : mix(hsl.z, 0.0, -uLightness);
            hsl.y = clamp(hsl.y * (1.0 + uSaturation), 0.0, 1.0);
            vec3 v = clamp(toRgb(hsl), 0.0, 1.0);
            gl_FragColor = vec4(v * a, a);
        }
