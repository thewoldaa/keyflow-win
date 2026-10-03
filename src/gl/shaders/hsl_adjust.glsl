precision mediump float;
uniform sampler2D uTexture;
uniform float uHue;
uniform float uSaturation;
uniform float uLightness;
varying vec2 vTex;
void main() {
    vec4 c = texture2D(uTexture, vTex);
    // The layer arrives premultiplied; colour math needs straight RGB, or a
    // semi-transparent edge rotates toward black instead of its own hue.
    float a = c.a;
    vec3 rgb = clamp(c.rgb / max(a, 1e-4), 0.0, 1.0);

    float ang = uHue * 3.14159265;
    float cs = cos(ang);
    float sn = sin(ang);
    // The standard luma-preserving hue rotation, written as three dots
    // rather than a mat3 so the row/column order cannot be read wrongly.
    vec3 hs = vec3(
        dot(rgb, vec3(0.299 + 0.701 * cs + 0.168 * sn,
                      0.587 - 0.587 * cs + 0.330 * sn,
                      0.114 - 0.114 * cs - 0.497 * sn)),
        dot(rgb, vec3(0.299 - 0.299 * cs - 0.328 * sn,
                      0.587 + 0.413 * cs + 0.035 * sn,
                      0.114 - 0.114 * cs + 0.292 * sn)),
        dot(rgb, vec3(0.299 - 0.300 * cs + 1.250 * sn,
                      0.587 - 0.588 * cs - 1.050 * sn,
                      0.114 + 0.886 * cs - 0.203 * sn))
    );

    float luma = dot(hs, vec3(0.2126, 0.7152, 0.0722));
    hs = mix(vec3(luma), hs, 1.0 + uSaturation);
    // Lightness lifts toward white or crushes toward black, as AE's does —
    // NOT a multiply, which would only ever darken.
    hs = (uLightness >= 0.0)
        ? mix(hs, vec3(1.0), uLightness)
        : mix(hs, vec3(0.0), -uLightness);

    hs = clamp(hs, 0.0, 1.0);
    gl_FragColor = vec4(hs * a, a);
}
