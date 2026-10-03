#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

        uniform sampler2D uTexture;
        uniform mat3 uLayerToLocal; // target uv -> the LAYER's own space
        uniform mat3 uLocalToUv;    // …and back again
        uniform float uTileCenterX;
        uniform float uTileCenterY;
        uniform float uTileWidth;
        uniform float uTileHeight;
        uniform float uOutputWidth;
        uniform float uOutputHeight;
        uniform float uMirrorEdges;
        uniform float uPhase;
        uniform float uHorizontalPhaseShift;
        // The layer drawn again, FLAT: its own 0..1 space IS this texture's uv (t
        // down the layer = v down the picture, so v is flipped), with ALL of it there
        // whatever the frame cut off. uWideCopy is 1 when it exists; 0 — which is also
        // what an unset uniform reads as — means the layer fitted the frame anyway and
        // the ordinary source is complete. The thumbnail renderer draws no copy and
        // must not be made to look for one. See the note about clipped tiles above.
        uniform sampler2D uWideSource;
        uniform float uWideCopy;
        uniform vec2 uWideTexelSize;  // 1/w, 1/h of the copy
        // Where the layer's own box sits inside the copy — (kx, ky, ox, oy), layer
        // (s, t) at copy (s*kx + ox, t*ky + oy). The copy is padded by whatever the
        // layer draws OUTSIDE its box (a stroke, glyph overhang), for the Motion
        // passes that read it; a tile only ever wants the box. All-zero (unset) means
        // no padding.
        uniform vec4 uWideFit;
        // What the Motion passes ABOVE this one did to the picture, as the map from a
        // layer point to the layer point whose pixel now sits there — in the LAYER's
        // own space. When it is on (uPreMotionOn = 1; 0 is also what unset reads as)
        // the tile reads the layer BEFORE those passes — the flat copy, or uOriginal —
        // through the map, instead of reading their moved pixels out of the scratch:
        //  - the flat copy is the layer's untouched content, so without the map a
        //    Twitch above Motion Tile shook a scratch this path never reads and the
        //    tiles stood still ("tidak kerender ketika di gerakin", any layer bigger
        //    than the frame);
        //  - and the scratch is frame-sized, so a turn (Swing, Twitch's Rotation) had
        //    already lost whatever it swung past the frame edge, and the whole-tile
        //    neighbour search cannot undo a rotation — every tile of a swinging
        //    full-frame photo came back with a black wedge cut out of it.
        // The map is exact for all three, so the search then only has to find
        // periodic neighbours, which it can. Only bound when every pass above this one
        // is a Motion pass (or the copy is in use, where nothing else reaches anyway).
        uniform sampler2D uOriginal;
        uniform mat3 uPreMotion;
        uniform float uPreMotionOn;
        // What the Motion passes BELOW this one would do to the picture, as their
        // sampling map in screen uv (output uv -> the uv they read from). When it is
        // on those passes are not run at all: this pass evaluates its plane at the
        // mapped point instead, which is the same picture with no frame edge in it —
        // the plane is infinite, the scratch those passes slid was not. 0 (unset) =
        // nothing folded, evaluate at vTex.
        uniform mat3 uPostMotion;
        uniform float uPostMotionOn;
        // 1 - the opacity of every Transform folded into this pass (above or below):
        // stated as a fade so that 0 — what unset reads as — means "no fade".
        uniform float uFoldFade;
        uniform vec4 uLayerRect;   // the layer's box in target uv
        uniform vec2 uTexelSize;   // 1/w, 1/h of this target
        // 1 when an earlier pass in this stack MOVES the picture (the Motion family,
        // Transform). See the neighbour search at the bottom — the shader is correct
        // either way, this only says whether it is worth looking.
        uniform float uNeighbourFill;
        varying vec2 vTex;
        // A WARP folded in from below (Wave Warp, Bulge — any pass with a
        // samplingMap) cannot be a 3x3 like uPostMotion, so the renderer compiles a
        // VARIANT of this shader with the warp's own sampling function spliced in
        // here, and its call spliced in at the second marker in main. The plane is
        // then evaluated at the warped point, exactly as for a folded Motion pass,
        // and the warp pass is skipped — run after the tile it read a frame-sized
        // picture and cut the warped edge off. See gl/TileFold.
        // %FOLD_DECLS%

        // One tap, in the LAYER's own 0..1 space: where was that layer point drawn, and
        // what is there? Outside the layer's box this is normally nothing at all —
        // which is exactly what makes the neighbour search below safe.
        vec4 tapLocal(vec2 local) {
            vec2 p = local;
            if (uPreMotionOn > 0.5) {
                // Which layer point did the passes above put here? Asked of the map,
                // because what is read below is the layer BEFORE they ran.
                vec3 m = uPreMotion * vec3(local, 1.0);
                if (m.z <= 0.0) return vec4(0.0);
                p = m.xy / m.z;
            }
            // A layer bigger than the composition left most of itself outside the
            // frame, and the frame is all the ordinary source holds. Read the flat
            // copy instead — for the WHOLE tile, not just the missing parts, so one
            // tile cannot be half sharp and half soft. It holds nothing but the
            // layer's box, so outside it there is nothing, exactly as in the scratch.
            if (uWideCopy > 0.5) {
                if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return vec4(0.0);
                vec4 fit = uWideFit.x > 0.0 ? uWideFit : vec4(1.0, 1.0, 0.0, 0.0);
                return texture2D(uWideSource, vec2(p.x * fit.x + fit.z, 1.0 - (p.y * fit.y + fit.w)));
            }
            vec3 r = uLocalToUv * vec3(p, 1.0);
            if (r.z <= 0.0) return vec4(0.0);
            vec2 uv = r.xy / r.z;
            // The source texture is CLAMP_TO_EDGE; sampling outside it would smear
            // the border pixels across the tile instead of showing nothing.
            if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return vec4(0.0);
            // With the map on, the moved pixels in the scratch are not wanted: the
            // layer as it was drawn is, and the map says where in it to look.
            if (uPreMotionOn > 0.5) return texture2D(uOriginal, uv);
            return texture2D(uTexture, uv);
        }

        void main() {
            // The point this pixel shows: vTex, or — with Motion passes folded in
            // below — wherever they would have read this pixel from. Everything
            // after this line is the plane evaluated at `uv`, so the whole field
            // moves with them and never runs out at the frame edge.
            vec2 uv = vTex;
            // %FOLD_APPLY%
            if (uPostMotionOn > 0.5) {
                vec3 m = uPostMotion * vec3(vTex, 1.0);
                if (abs(m.z) < 1e-6) {
                    gl_FragColor = vec4(0.0);
                    return;
                }
                uv = m.xy / m.z;
            }

            // Outside the output rectangle nothing is drawn at all — that is how
            // Output W/H shrink the result rather than scaling it. Measured on the
            // FRAME (vTex), never on the mapped point: at the default 100% this
            // rectangle IS the frame, and cropping at the mapped point would cut away
            // exactly the pixels the fold brings in from beyond it — the fold's first
            // build did that, and the field still ran out at the frame edge. The
            // field moves under a fixed window, as it does when the layer is dragged.
            vec2 halfOutput = vec2(uOutputWidth, uOutputHeight) * 0.5;
            vec2 fromCentre = abs(vTex - 0.5);
            if (fromCentre.x > halfOutput.x || fromCentre.y > halfOutput.y) {
                gl_FragColor = vec4(0.0);
                return;
            }

            // Where this screen pixel falls in the LAYER's own 0..1 space. Everything
            // below happens in that space, which is what makes the tiled plane turn
            // and tilt WITH the layer: the grid is nailed to the layer, not to the
            // screen. Tiling in uv instead left an axis-aligned screen grid whose
            // cells each held a separately-rotated copy — under a 3D rotation that
            // read as every duplicate spinning about its own anchor.
            vec3 q = uLayerToLocal * vec3(uv, 1.0);
            if (abs(q.z) < 1e-4) {
                // Two very different things arrive here. A degenerate matrix (a
                // zero-size or edge-on layer) is all zeros, so q is zero too: there is
                // no frame to work in, hand the pixel through untouched. A real
                // vanishing w is the HORIZON of a tilted plane, where q.xy has run off
                // to infinity — past it there is no layer point at all.
                bool inside = uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0;
                gl_FragColor = (dot(q.xy, q.xy) < 1e-8 && inside)
                    ? texture2D(uTexture, uv) : vec4(0.0);
                return;
            }
            if (q.z < 0.0) {          // behind the viewer, on the far side of the horizon
                gl_FragColor = vec4(0.0);
                return;
            }
            vec2 local = q.xy / q.z;

            // A tile is the layer scaled by Tile W/H — so in LAYER space the tile size
            // IS Tile W/H, and Tile Center is already expressed in the same units.
            vec2 tile = max(vec2(uTileWidth, uTileHeight), vec2(1e-4));
            // Position in TILE units: 0 at a tile's centre, ±0.5 at its edges.
            vec2 p = (local - vec2(uTileCenterX, uTileCenterY)) / tile;

            // Phase offsets every other row (or column) along the tiling axis,
            // which is what turns a grid into a brick pattern.
            float phase = uPhase / 360.0;
            if (uHorizontalPhaseShift > 0.5) {
                p.y += phase * floor(p.x + 0.5);
            } else {
                p.x += phase * floor(p.y + 0.5);
            }

            vec2 cell = floor(p + 0.5);
            vec2 f = p - cell;                       // -0.5 .. 0.5 inside the tile
            if (uMirrorEdges > 0.5) {
                // Flip odd cells so neighbouring tiles meet edge-to-edge.
                vec2 odd = mod(abs(cell), 2.0);
                f = mix(f, -f, step(0.5, odd));
            }

            // The WHOLE layer goes into EVERY tile — f spans the layer, not the tile —
            // so Tile W/H at 50% is the layer at half size repeated twice: the video
            // wall Motion Tile exists to make. Sampling a tile-sized window at 1:1
            // instead CROPS: at 50% each tile showed only the middle half of the layer
            // and the rest was never drawn anywhere. It also made Tile W/H above 100%
            // sample outside the layer (transparent) where it should zoom the picture
            // up, and it contradicted this effect's own thumbnail, which promises nine
            // small WHOLE copies.
            // Half a texel INSIDE the layer, never ON its edge. A tile seam samples
            // exactly that edge, and a linear tap there mixes in the transparent margin
            // around the layer — a pale hairline along every seam. Invisible as a trim
            // (it is half a SOURCE texel however small the tile is), fatal as a seam:
            // this is the "putih putih" that appeared along the top and bottom once the
            // wide copy moved the layer's edge off the frame border. In the flat copy
            // the layer fills the texture edge to edge, so the texel is the copy's own.
            // (In a padded copy the layer's box is a fraction of the texture, so
            // half a copy texel is that much MORE in layer units.)
            vec2 fitK = uWideFit.x > 0.0 ? uWideFit.xy : vec2(1.0);
            vec2 inset = uWideCopy > 0.5
                ? 0.5 * uWideTexelSize / fitK
                : 0.5 * uTexelSize / max(uLayerRect.zw - uLayerRect.xy, vec2(1e-4));
            vec2 sampleLocal = clamp(vec2(0.5) + f, inset, vec2(1.0) - inset);
            // Back out of layer space: where was that layer point actually drawn?
            vec4 c = tapLocal(sampleLocal);

            // THE TILING DOES NOT STOP AT THE LAYER'S BOX. An earlier pass that moves
            // the picture — Oscillate, Twitch, Swing, Transform — slides it partly OUT
            // of that box, and the part that left is simply missing from the scratch at
            // the point this tile asks for: every tile came back with a bite out of it,
            // reported as "kalau di layer ada motion tiles dan di layer itu di tambahkan
            // salah satu dari effect di motion, gambar/object nya akan terpotong".
            //
            // But the plane is PERIODIC, so what left one side of the box is exactly
            // what should arrive at the other: it is sitting one tile over, where the
            // earlier pass put it. Looking there turns the gaps into a seamless,
            // endlessly scrolling tiled field — which is what "move a tiled plane"
            // means, and what After Effects gives you for the same effect order.
            //
            // Safe on its own: outside the layer's box the scratch holds NOTHING unless
            // some pass moved pixels there, so a layer with transparent holes and no
            // motion under it finds nothing and keeps its holes. uNeighbourFill is
            // therefore only about cost — 8 extra taps on a pixel that came back empty.
            if (uNeighbourFill > 0.5) {
                for (int j = -1; j <= 1; j++) {
                    for (int i = -1; i <= 1; i++) {
                        if (c.a <= 0.0) {
                            vec4 n = tapLocal(sampleLocal + vec2(float(i), float(j)));
                            if (n.a > 0.0) c = n;
                        }
                    }
                }
            }
            // Premultiplied throughout, so a folded Transform's opacity is one scale.
            gl_FragColor = c * (1.0 - uFoldFade);
        }
