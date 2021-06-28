#version 330 core

#alp include <ToneMapping/exposure>
#alp include <Blending/screen>

out vec3 fragColor;
in vec2 texCoord;

uniform sampler2D bloom;
uniform sampler2D dof;
uniform sampler2D image;
uniform sampler2D cocMap;
uniform float dofSigmaDivider;
uniform float exposure;
uniform float gamma;

vec3 blendDOF(vec3 base, vec3 dof) {
    float k = texture(cocMap, texCoord).r / dofSigmaDivider;

    if (k > 1.0f) {
        return dof;
    }

    return mix(base, dof, k);
}

void main() {
    vec3 color = blendDOF(texture(image, texCoord).rgb, texture(dof, texCoord).rgb);
    
    color = blendScreen(texture(bloom, texCoord).rgb, color);
    
    fragColor = tonemapExposure(color, exposure);
    fragColor = pow(fragColor, vec3(1.0f / gamma)); // gamma correction
}
