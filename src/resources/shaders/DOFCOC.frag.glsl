in vec2 texCoord;

layout (location = 0) out float fragCoC;

#alp include <DOF/cinematicCoC>

uniform sampler2D positionMap;

uniform float planeInFocus;
uniform float aperture;
uniform float imageDistance;

void main() {
    float sigma = cinematicCoC(
        texture(positionMap, texCoord).z,
        planeInFocus,
        aperture,
        imageDistance
    );
    fragCoC = abs(sigma);
}
