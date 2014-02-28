#version 150

uniform mat4 ProjectionMatrix;
uniform mat4 ModelViewMatrix;
uniform mat3 NormalMatrix;
uniform vec4 LightPosition;

in vec4 vPosition;
in vec3 vNormal;
in vec3 vTangent;
in vec2 vTexCoord;

out vec3 fView;
out vec3 fLight;
out vec2 fTexCoord;

void main()
{
    // Tangent space vectors give the columns of the eye-to-tangent transform.

    vec3 N = NormalMatrix * vNormal;
    vec3 T = NormalMatrix * vTangent;
    mat3 M = transpose(mat3(T, cross(N, T), N));

    // Compute the per-fragment attributes.

    fView     =  M * vec3(ModelViewMatrix * vPosition);
    fLight    =  M * vec3(ModelViewMatrix * LightPosition);
    fTexCoord =  vTexCoord;

    gl_Position = ProjectionMatrix * ModelViewMatrix * vPosition;
}
