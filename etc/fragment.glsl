#version 150

uniform vec4 AmbientLight;

uniform sampler2D NormalTexture;
uniform sampler2D DiffuseTexture;
uniform sampler2D SpecularTexture;

in vec3 fView;
in vec3 fLight;
in vec2 fTexCoord;

out vec4 fColor;

void main()
{
    // Sample the textures.

    vec4 tN = texture(NormalTexture,   fTexCoord);
    vec4 tD = texture(DiffuseTexture,  fTexCoord);
    vec4 tS = texture(SpecularTexture, fTexCoord);

    // Determine the per-fragment lighting vectors.

    vec3 N = normalize(2.0 * tN.xyz - 1.0);
    vec3 L = normalize(fLight);
    vec3 V = normalize(fView);
    vec3 R = reflect(L, N);

    // Compute the diffuse shading.

    float kd =     max(dot(L, N), 0.0);
    float ks = pow(max(dot(V, R), 0.0), 8.0);

    // Calculate the fragment color.

    fColor.rgb = vec3(AmbientLight * tD + kd * tD + tS * ks);
    fColor.a   = tD.a;
}
