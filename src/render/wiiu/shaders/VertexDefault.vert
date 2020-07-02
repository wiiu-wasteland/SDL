uniform mat4 u_projection;
attribute vec2 a_position;
attribute vec2 a_texCoord;
attribute vec2 a_angle;
attribute vec2 a_center;
varying vec2 v_texCoord;

void main()
{
    float s = a_angle[0];
    float c = a_angle[1] + 1.0;
    mat2 rotationMatrix = mat2(c, -s, s, c);
    vec2 position = rotationMatrix * (a_position - a_center) + a_center;
    v_texCoord = a_texCoord;
    gl_Position = u_projection * vec4(position, 0.0, 1.0);
    gl_PointSize = 1.0;
}
