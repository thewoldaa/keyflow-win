attribute vec2 aPos;
attribute vec2 aTex;
varying vec2 vTex;
void main() {
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
    vTex = aTex;
}
