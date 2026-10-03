attribute vec4 aPos;
attribute vec4 aTex;
uniform mat4 uSt;
varying vec2 vTex;
void main() { gl_Position = aPos; vTex = (uSt * aTex).xy; }
