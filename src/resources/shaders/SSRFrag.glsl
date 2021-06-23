#alp include <SSR>

uniform sampler2D baseImage;
uniform sampler2D normalMap; // in view space
uniform sampler2D ssrValuesMap;
uniform sampler2D positionMap; // in view space
uniform mat4 projection;
uniform mat4 view;

layout (location = 0) out vec3 fragColor;

in vec2 texCoord;

void main() {
    SSRValues values;
    values.fallbackColor = vec3(0.0);
    values.uv = texCoord;
    values.projection = projection;
    values.view = view;
    values.reflectionStrength = texture(ssrValuesMap, texCoord).r;
    values.jitter = texture(ssrValuesMap, texCoord).g;
    values.rayMarchCount = 30;
    values.binarySearchCount = 10;
    values.rayStep = 0.05f;
    values.LLimiter = 0.1f;
    values.minRayStep = 0.2f;

    fragColor = ssrGetColor(baseImage, normalMap, positionMap, values);
}
