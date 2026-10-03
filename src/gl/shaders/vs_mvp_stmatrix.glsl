uniform mat4 uMVP;
uniform mat4 uSTMatrix;
attribute vec2 aPos;
attribute vec2 aTex;
varying vec2 vTex;
void main() {
    gl_Position = uMVP * vec4(aPos, 0.0, 1.0);
    vTex = (uSTMatrix * vec4(aTex, 0.0, 1.0)).xy;
}
