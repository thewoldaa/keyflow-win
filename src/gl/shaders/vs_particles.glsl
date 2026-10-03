precision highp float;

attribute vec3 aParticle;   // x = slot index, yz = quad corner in -1..1

uniform mat4 uVP;
// The view-projection and the emitter as they were at the START of the shutter
// window. Motion blur is measured against these, so a moving CAMERA and a moving
// EMITTER smear the particles exactly as their own flight does. Equal to the pair
// above when neither moved.
uniform mat4 uVPPrev;
uniform vec3 uOriginPrev;
// HALF the shutter window, in seconds. 0 = the layer's motion-blur switch is off,
// and the whole block below is skipped.
uniform float uShutter;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
uniform vec3 uOrigin;       // emitter, composition space (+Z away, AE sign)
uniform vec2 uComp;         // composition size in px — emitter size is a fraction of it
uniform float uTime;        // seconds since the layer's in point
uniform float uRate;        // particles per second the user asked for (clamped)
uniform float uLattice;     // slots per second — fixed, independent of uRate
uniform float uPool;        // slots in the pool

uniform float uPreRoll;
uniform float uPosX, uPosY, uPosZ;
uniform float uEmitterW, uEmitterH, uEmitterD, uEmitterSphere;
uniform float uVelocity, uVelocityRandom, uDirTilt, uDirSpin, uSpread, uOutwards;
uniform float uGravity, uWindX, uWindY, uWindZ, uDrag, uTurbulence, uTurbSpeed;
uniform float uLife, uLifeRandom, uSize, uSizeRandom, uSizeEnd, uStretch;
uniform float uOpacity, uFadeIn, uFadeOut;
uniform float uColorR, uColorG, uColorB, uColorRandom;
uniform float uColorEndR, uColorEndG, uColorEndB;
uniform float uSeed;

varying vec2 vCorner;
varying vec4 vColor;

const float TAU = 6.2831853;

// Three uncorrelated 0..1 values from one number (Hoskins' hash — cheap, and stable
// across drivers because it is pure float arithmetic with no sin()).
vec3 hash3(float p) {
    vec3 p3 = fract(vec3(p) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yxz + 33.33);
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

vec3 hsv2rgb(vec3 c) {
    vec3 k = abs(fract(c.xxx + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
    return c.z * mix(vec3(1.0), clamp(k - 1.0, 0.0, 1.0), c.y);
}

/**
 * Displacement from the launch point after `dt` seconds of flight — the closed form
 * the whole system rests on, pulled out as a function because motion blur has to
 * evaluate it a SECOND time, at the start of the shutter window. That is the payoff
 * of a stateless simulation: "where was this particle a 200th of a second ago" is a
 * function call, not a history buffer.
 *
 * `damp` is the integral of exp(-drag*t); the series form near zero keeps it finite
 * where the exact expression divides 0 by 0, so drag can be keyframed up from
 * nothing without a discontinuity.
 *
 * Turbulence is NOT here — it is its own function below, because motion blur has to
 * evaluate it at both instants too. See [turbulenceOffset].
 */
vec3 flightOffset(float dt, vec3 dir, float speed) {
    float drag = max(uDrag, 0.0);
    float damp = (drag * dt < 1e-3) ? dt * (1.0 - 0.5 * drag * dt)
                                    : (1.0 - exp(-drag * dt)) / drag;
    return dir * speed * damp
         + vec3(uWindX, uWindY, uWindZ) * dt
         + vec3(0.0, uGravity, 0.0) * (0.5 * dt * dt);
}

/**
 * The turbulent wander, at absolute time [clock] and life fraction [kk].
 *
 * Per-particle wander rather than a sampled noise field: a field would have to be
 * evaluated at the CURRENT position, and the current position is what it is
 * displacing — a loop no closed form can carry. Summed sines at incommensurable rates
 * never repeat over any usable timeline, and each particle gets its own phase, so the
 * spray breaks up instead of swaying as one body. Ramped in over the first half of
 * life so nothing pops at birth.
 *
 * **It is a function of TIME, so motion blur must evaluate it at both ends of the
 * shutter and take the difference.** Leaving it out of the earlier position instead
 * makes the blur count this whole displacement as travel — tens of pixels of
 * per-particle noise, which swamps the few pixels the particle actually moved and
 * sprays streaks in random directions.
 */
vec3 turbulenceOffset(float clock, float kk, vec3 phase, float generation) {
    if (uTurbulence <= 0.0) return vec3(0.0);
    float w = uTurbSpeed * (clock + generation * 3.7);
    vec3 wander = vec3(
        sin(w + phase.x) + 0.5 * sin(2.13 * w + phase.y),
        sin(1.17 * w + phase.y) + 0.5 * sin(2.31 * w + phase.z),
        sin(0.93 * w + phase.z) + 0.5 * sin(1.87 * w + phase.x)
    );
    return uTurbulence * wander * min(kk * 2.0, 1.0);
}

void main() {
    float idx = aParticle.x;
    vec2 corner = aParticle.yz;

    // Which emission this slot is on, and how long ago it started. Births are
    // staggered by index so the stream is continuous rather than one pulse per
    // cycle; the pool is sized (on the CPU) so one cycle outlasts one life.
    // Pre-roll: the simulation is already this far along at the layer's in point.
    // Free, and only a closed-form system can offer it — there is no state to fast
    // forward, so "already running for 2 seconds" is an addition. It is what makes a
    // full screen of particles exist at the FIRST frame instead of building up from
    // an empty frame every time the layer starts.
    float now = uTime + uPreRoll * 0.001;
    float lattice = max(uLattice, 1e-4);
    float cycle = max(uPool / lattice, 1e-4);
    float since = now - idx / lattice;
    float generation = floor(since / cycle);
    float age = since - generation * cycle;

    // Randomness is per (slot, generation): a respawned particle is a NEW particle,
    // not the same one replayed.
    float sid = idx * 1.7 + generation * 91.7 + uSeed * 13.31;
    vec3 r1 = hash3(sid);           // life, cone
    vec3 r2 = hash3(sid + 37.13);   // emitter offset
    vec3 r3 = hash3(sid + 71.77);   // sphere direction, turbulence phase
    vec3 r4 = hash3(sid + 113.7);   // speed, size, hue
    vec3 r5 = hash3(sid + 191.3);   // emission lottery

    float life = max(uLife, 1.0) * 0.001 * (1.0 + (r1.x - 0.5) * 2.0 * uLifeRandom);
    life = max(life, 1e-3);
    float k = age / life;

    // The rate as a fraction of the lattice, drawn against this slot's own fixed
    // number: the slots stay where they are and the rate only decides how many of
    // them fire. That is what lets Particles/second be keyframed without the whole
    // spray re-timing itself, and it makes the emission irregular the way a real
    // spray is, rather than a metronome ticking at exactly 1/rate.
    bool emitted = r5.x < uRate / lattice;

    // Never fired, not born yet, or already dead: collapse behind the near plane so
    // the quad is clipped away entirely. (All six vertices agree — same slot index.)
    if (!emitted || since < 0.0 || k > 1.0) {
        gl_Position = vec4(0.0, 0.0, -2.0, 1.0);
        vColor = vec4(0.0);
        vCorner = vec2(0.0);
        return;
    }

    // Where in the emitter it starts. Zero size collapses to a point emitter; the
    // size is a FRACTION OF THE FRAME, so 100% is exactly one screen across and a
    // project re-rendered at another resolution emits over the same region. (Depth
    // has no frame dimension of its own and uses the height, the same convention
    // Wave Warp and Bulge measure by.)
    vec3 halfSize = vec3(uEmitterW * uComp.x, uEmitterH * uComp.y, uEmitterD * uComp.y) * 0.5;
    vec3 unit = r2 * 2.0 - 1.0;
    if (uEmitterSphere > 0.5) {
        // Uniform on the sphere via the cos-z / phi construction, then pushed to a
        // uniform density inside it. Normalising a random cube point instead piles
        // particles up at the eight corners, which reads as a visibly lumpy ball.
        float cz = r3.x * 2.0 - 1.0;
        float sr = sqrt(max(0.0, 1.0 - cz * cz));
        float ph = TAU * r3.y;
        unit = vec3(sr * cos(ph), sr * sin(ph), cz) * pow(max(r2.x, 1e-4), 1.0 / 3.0);
    }
    vec3 offset = unit * halfSize;

    // Emission direction: a cone of half-angle uSpread around the aim. Composition
    // space is y-DOWN, so "up the screen" is -Y and a tilt of 0 is straight up.
    float tilt = radians(uDirTilt);
    float spin = radians(uDirSpin);
    vec3 aim = vec3(sin(tilt) * sin(spin), -cos(tilt), sin(tilt) * cos(spin));
    // Uniform over the spherical cap: the COSINE is what must be uniform, not the
    // angle, or the spray bunches along its axis.
    float cosMax = cos(radians(clamp(uSpread, 0.0, 180.0)));
    float cz2 = mix(1.0, cosMax, r1.y);
    float sz2 = sqrt(max(0.0, 1.0 - cz2 * cz2));
    float ph2 = TAU * r1.z;
    vec3 pole = abs(aim.y) > 0.99 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 tAxis = normalize(cross(pole, aim));
    vec3 bAxis = cross(aim, tAxis);
    vec3 dir = normalize(tAxis * (sz2 * cos(ph2)) + bAxis * (sz2 * sin(ph2)) + aim * cz2);

    float offLen = length(offset);
    if (uOutwards > 0.001 && offLen > 0.001) {
        dir = normalize(mix(dir, offset / offLen, clamp(uOutwards, 0.0, 1.0)));
    }

    float speed = uVelocity * max(0.0, 1.0 + (r4.x - 0.5) * 2.0 * uVelocityRandom);

    float dt = age;
    vec3 phase = r3 * TAU;
    vec3 emitterOffset = vec3(uPosX * uComp.x, uPosY * uComp.y, uPosZ * uComp.y) + offset;
    vec3 pos = uOrigin + emitterOffset + flightOffset(dt, dir, speed)
             + turbulenceOffset(now, k, phase, generation);

    float size = uSize * max(0.05, 1.0 + (r4.y - 0.5) * 2.0 * uSizeRandom)
               * mix(1.0, uSizeEnd, k);
    float fadeIn = uFadeIn <= 0.0 ? 1.0 : smoothstep(0.0, uFadeIn, k);
    float fadeOut = uFadeOut <= 0.0 ? 1.0 : 1.0 - smoothstep(1.0 - uFadeOut, 1.0, k);

    // Colour travels from its start to its end across the particle's own life —
    // which is what an ember cooling from white through orange to red IS. Both
    // swatches white leaves it a plain constant colour, so it costs nothing until
    // the second one is moved.
    vec3 color = mix(vec3(uColorR, uColorG, uColorB), vec3(uColorEndR, uColorEndG, uColorEndB), k);
    if (uColorRandom > 0.0) {
        color = mix(color, hsv2rgb(vec3(r4.z, 0.85, 1.0)), clamp(uColorRandom, 0.0, 1.0));
    }
    vColor = vec4(color, uOpacity * fadeIn * fadeOut);
    vCorner = corner;

    // Billboard axes. Normally the camera's own right/up, so the sprite always faces
    // the lens; with Stretch, the quad is turned to lie ALONG the particle's motion
    // and lengthened by how fast it is going, which is what turns a dot into a spark
    // or a raindrop. The velocity is the analytic derivative of the position above,
    // so it costs no extra state.
    vec3 axisU = uCamRight;
    vec3 axisV = uCamUp;
    float sizeV = size;
    if (uStretch > 0.0) {
        vec3 vel = dir * speed * exp(-max(uDrag, 0.0) * dt)
                 + vec3(uWindX, uWindY, uWindZ)
                 + vec3(0.0, uGravity, 0.0) * dt;
        // Only the part of the velocity the camera can SEE stretches the sprite:
        // one flying straight at the lens has no screen direction to stretch along,
        // and must stay a dot rather than smear in an arbitrary direction.
        vec2 vs = vec2(dot(vec3(vel.x, vel.y, -vel.z), uCamRight),
                       dot(vec3(vel.x, vel.y, -vel.z), uCamUp));
        float vlen = length(vs);
        if (vlen > 1e-3) {
            vec2 d = vs / vlen;
            axisV = uCamRight * d.x + uCamUp * d.y;    // along the motion
            axisU = uCamRight * -d.y + uCamUp * d.x;   // across it
            // Measured against the launch speed, so the amount means the same thing
            // whatever units the scene is built in. Clamped so a particle caught in
            // a hurricane wind does not become a line across the frame.
            sizeV = size * clamp(1.0 + uStretch * vlen / max(uVelocity, 1.0),
                                 1.0, 1.0 + uStretch * 6.0);
        }
    }

    // Composition Z is AE's (+Z away); GL's is the opposite.
    vec3 world = vec3(pos.x, pos.y, -pos.z);

    // ── Motion blur ────────────────────────────────────────────────────────────
    // Per PARTICLE, not per layer, because that is the only place the motion is:
    // a particle system's movement lives in the particles, so the layer's transform
    // — which is what the compositor's velocity-blur pass measures — is perfectly
    // still while the frame is full of streaks.
    //
    // The apparent travel is measured where it is actually seen: on SCREEN, between
    // the particle's own position a shutter ago through the camera of that instant,
    // and its position now through the camera of now. One subtraction therefore
    // carries all three sources at once — the particle's flight, a moving emitter,
    // and a moving camera, the last of them WITH parallax (a mote near the lens
    // smears further under a pan than one far away, which a single whole-layer
    // velocity field cannot express).
    vec4 cNow = uVP * vec4(world, 1.0);
    // Skipped for anything at or behind the lens: w is the distance in front of the
    // camera, and dividing by a w near zero throws the projected position off to
    // infinity — the difference of two such numbers is noise, and noise here means a
    // streak pointing somewhere the particle never went.
    if (uShutter > 0.0 && cNow.w > uComp.y * 0.02) {
        float dtPrev = max(dt - uShutter, 0.0);
        vec3 posPrev = uOriginPrev + emitterOffset
                     + flightOffset(dtPrev, dir, speed)
                     // Evaluated at the EARLIER instant, so only the turbulence's
                     // CHANGE across the shutter counts as travel. Reusing the
                     // current wander here would hand the blur tens of pixels of
                     // per-particle noise as if the particle had moved that far.
                     + turbulenceOffset(now - uShutter, dtPrev / life, phase, generation);
        vec4 cPrev = uVPPrev * vec4(vec3(posPrev.x, posPrev.y, -posPrev.z), 1.0);
        if (cPrev.w > uComp.y * 0.02) {
            vec2 nNow = cNow.xy / cNow.w;
            // Doubled: uShutter is the HALF window, and the camera is interpolated
            // linearly across it, so start->frame is exactly half of start->end.
            vec2 dNdc = (nNow - cPrev.xy / cPrev.w) * 2.0;
            // Screen motion back into world units, measured against the camera's own
            // axes AT THIS DEPTH. A world step along uCamRight lands on clip x alone
            // and uCamUp on clip y alone — that is what makes them the camera's axes
            // — so the two scales are independent and no general solve is needed,
            // and the y flip in the projection is accounted for by construction.
            vec4 cU = uVP * vec4(world + uCamRight, 1.0);
            vec4 cV = uVP * vec4(world + uCamUp, 1.0);
            float perU = cU.x / cU.w - nNow.x;
            float perV = cV.y / cV.w - nNow.y;
            vec2 travel = vec2(dNdc.x / (abs(perU) < 1e-9 ? 1e-9 : perU),
                               dNdc.y / (abs(perV) < 1e-9 ? 1e-9 : perV));
            // Capped at a frame height: past that the streak is neither believable
            // nor affordable — it is a full-frame quad per particle.
            float mbLen = min(length(travel), uComp.y);
            if (mbLen > max(size, 1.0) * 0.05) {
                vec2 a = normalize(travel);
                axisV = uCamRight * a.x + uCamUp * a.y;   // along the apparent motion
                axisU = uCamRight * -a.y + uCamUp * a.x;  // across it
                sizeV += mbLen;
                // A real shutter spreads the SAME light over the whole streak, so a
                // sprite drawn n times longer must be n times dimmer. Without this,
                // turning motion blur on makes a fast spray brighter, not blurrier.
                vColor.a *= size / (size + mbLen);
            }
        }
    }

    // Billboarding in WORLD space along those axes — rather than offsetting the
    // projected point — is what gives the sprite true perspective: one that drifts
    // toward the camera grows because it is nearer, not because a uniform said so.
    world += axisU * (corner.x * size * 0.5) + axisV * (corner.y * sizeV * 0.5);
    gl_Position = uVP * vec4(world, 1.0);
}
