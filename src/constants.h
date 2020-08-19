#ifndef ALGINE_EXAMPLES_CONSTANTS_H
#define ALGINE_EXAMPLES_CONSTANTS_H

#include <algine/types.h>

#define algineResources "lib/algine/resources/"
#define resources "src/resources/"

namespace algine {
constexpr uint pointLightsCount = 1;
constexpr uint dirLightsCount = 1;
constexpr uint pointLightsLimit = 4;
constexpr uint dirLightsLimit = 4;
constexpr uint maxBoneAttribsPerVertex = 1;
constexpr uint maxBones = 64;

constexpr uint shadowMapResolution = 1024;

constexpr float bloomK = 0.5f;
constexpr float dofK = 0.5f;
constexpr uint bloomBlurKernelRadius = 15;
constexpr uint bloomBlurKernelSigma = 16;
constexpr uint dofBlurKernelRadius = 2;
constexpr uint dofBlurKernelSigma = 4;
constexpr uint cocBlurKernelRadius = 2;
constexpr uint cocBlurKernelSigma = 6;
}

#endif //ALGINE_EXAMPLES_CONSTANTS_H
