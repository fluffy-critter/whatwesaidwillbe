// basic passthrough fragment color
#version 120

varying vec4 gl_Color;

void main() {
    gl_FragColor = gl_Color;
}
