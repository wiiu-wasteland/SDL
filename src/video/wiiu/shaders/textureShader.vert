/* textureShader: vertex shader (GLSL) */
/* compile with ShaderAnalyzer (rv770) + fix latte-assembler messages */

layout(location = 0) uniform vec2 u_viewSize; /* viewport size */
layout(location = 1) uniform vec2 u_texSize;  /* texture size */
layout(location = 0) in vec2 a_position;      /* vertex position */
layout(location = 1) in vec2 a_texCoordIn;    /* texture position */
out vec2 texCoord; /* relative texture postion passed to pixel shader */

void main()
{
    /* Compute relative vertex position */
    a_position.y = u_viewSize.y - a_position.y;
    a_position.x = ((a_position.x / u_viewSize.x) * 2.0f) - 1.0f;
    a_position.y = ((a_position.y / u_viewSize.y) * 2.0f) - 1.0f;

    /* Set vertex position */
    gl_Position = vec4(a_position, 0.0, 1.0);

    /* Compute relative coordinate position */
    a_texCoordIn.y = u_texSize.y - a_texCoordIn.y;
    a_texCoordIn.x = a_texCoordIn.x / u_texSize.x;
    a_texCoordIn.y = a_texCoordIn.y / u_texSize.y;

    /* Pass texture coordinate to pixel shader */
    texCoord = a_texCoordIn;
}
