#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D uTexture;
uniform vec2 uTexelSize;   // 1/w, 1/h of the target
uniform vec4 uLayerRect;   // the layer's box in target uv
uniform float uKeyR;
uniform float uKeyG;
uniform float uKeyB;
uniform float uTolerance;
uniform float uSoftness;
uniform float uPreBlur;
uniform float uClipBlack;
uniform float uClipWhite;
uniform float uSpill;
uniform float uSpillBias;
varying vec2 vTex;

vec3 ycc(vec3 c) {
    float y = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return vec3(y, (c.b - y) / 1.8556, (c.r - y) / 1.5748);
}
vec2 chroma(vec3 c) {
    float v = max(max(c.r, c.g), max(c.b, 1e-4));
    return ycc(c / v).yz;
}


void main() {
    vec4 base = texture2D(uTexture, vTex);
    if (base.a <= 0.0) {
        gl_FragColor = vec4(0.0);
        return;
    }
    vec3 p = base.rgb / base.a;
    vec3 K = vec3(uKeyR, uKeyG, uKeyB);

    // The colour the decision is made on: a ring of taps, alpha-weighted (summing
    // premultiplied colour and dividing by summed alpha IS the weighted mean), the
    // centre counted twice. Held inside the layer's box like every neighbour read.
    vec3 judged = p;
    if (uPreBlur > 0.0) {
        float rb = uPreBlur * (1.0 / 1080.0) / uTexelSize.y;   // texels
        vec4 acc = base * 2.0;
        for (int i = 0; i < 8; i++) {
            float ang = float(i) * 0.7853982;
            vec2 o = vec2(cos(ang), sin(ang)) * rb * uTexelSize;
            acc += texture2D(uTexture, clamp(vTex + o, uLayerRect.xy, uLayerRect.zw));
        }
        judged = acc.rgb / max(acc.a, 1e-4);
    }

    vec2 kc = chroma(K);
    float d = distance(chroma(judged), kc);
    // Keyed out inside the tolerance, kept beyond tolerance + softness, a smooth
    // ramp between. The floor keeps smoothstep's two edges apart at Softness 0,
    // where they would otherwise coincide and the result be undefined.
    float matte = smoothstep(uTolerance, uTolerance + max(uSoftness, 1e-4), d);
    matte = clamp((matte - uClipBlack) / max(uClipWhite - uClipBlack, 1e-4), 0.0, 1.0);
    if (matte <= 0.0) {
        gl_FragColor = vec4(0.0);
        return;
    }

    // The key's dominant channel — the one the screen lights up and spill leaks into.
    vec3 domSel = (K.g >= K.r && K.g >= K.b) ? vec3(0.0, 1.0, 0.0)
                : ((K.b >= K.r) ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0));

    // Un-mix: the foreground, premultiplied by the matte.
    float sAmt = 1.0 - matte;
    float screenScale = min(1.0, dot(p, domSel) / max(sAmt * dot(K, domSel), 1e-4));
    vec3 c = max(p - sAmt * K * screenScale, 0.0);

    // Spill: limit the dominant channel to a blend of the other two, luma restored.
    float dom = dot(c, domSel);
    vec3 others = c - dom * domSel;
    float oMax = max(max(others.r, others.g), others.b);
    vec3 oo = others + domSel * 1e9;
    float oMin = min(min(oo.r, oo.g), oo.b);
    float lim = mix(oMin, oMax, uSpillBias);
    // At an edge the un-mix can take out MORE green than the pixel's real share of
    // screen (the key is the screen at its brightest), which leaves the other two
    // channels standing: a magenta rim. Green is let back up to the spill limit —
    // never past the pixel's own green at this coverage, so nothing is invented.
    if (matte < 1.0) dom = max(dom, min(lim, dot(p, domSel) * matte));
    float excess = max(dom - lim, 0.0) * uSpill;
    float lost = excess * dot(domSel, vec3(0.2126, 0.7152, 0.0722));
    c = others + domSel * (dom - excess) + vec3(lost);
    c = min(c, vec3(matte));

    float a = base.a * matte;
    gl_FragColor = vec4(c * base.a, a);
}
