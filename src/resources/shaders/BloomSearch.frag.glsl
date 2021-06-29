#alp include <Luminance/luminance>

uniform float brightnessThreshold = 0.3;
uniform sampler2D image;

in vec2 texCoord;

layout (location = 0) out vec3 fragColor;

void main() {
    vec3 color = texture(image, texCoord).rgb;

    if (luminance(color) > brightnessThreshold) {
        fragColor = color;
    }
}
