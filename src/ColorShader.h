#ifndef COLORSHADER_H
#define COLORSHADER_H

#define constant(name, val) constexpr char name[] = val;

namespace ColorShader::Vars {
constant(ModelMatrix, "modelMatrix")
constant(ViewMatrix, "viewMatrix")
constant(MVPMatrix, "MVPMatrix")
constant(MVMatrix, "MVMatrix")
constant(InPos, "inPos")
constant(InNormal, "inNormal")
constant(InTexCoord, "inTexCoord")
constant(InTangent, "inTangent")
constant(InBitangent, "inBitangent")

constant(AmbientTex, "ambient")
constant(DiffuseTex, "diffuse")
constant(SpecularTex, "specular")
constant(NormalTex, "normal")
constant(ReflectionStrengthTex, "reflectionStrength")
constant(JitterTex, "jitter")
constant(AmbientStrength, "ambientStrength")
constant(DiffuseStrength, "diffuseStrength")
constant(SpecularStrength, "specularStrength")
constant(Shininess, "shininess")
}

#undef constant

#endif //COLORSHADER_H
