/*
 draw in polar coordinates; x becomes angle (degrees CCW from right),
 y becomes radius (z and w unaffected)

*/

#version 120

void main() {
    gl_Position = gl_ModelViewProjectionMatrix
        * vec4(cos(gl_Vertex.x)*gl_Vertex.y, sin(gl_Vertex.x)*gl_Vertex.y, gl_Vertex.zw);

    gl_FrontColor = gl_Color;
}

