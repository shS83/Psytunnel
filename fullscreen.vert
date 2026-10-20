#version 330 core

// Yksi suuri kolmio peittää ruudun. Sen jokaisen pikselin väri lasketaan
// fragment shaderissa, joten erillistä tunnelin kolmioverkkoa ei tarvita.
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(p*2.0-1.0, 0.0, 1.0);
}
