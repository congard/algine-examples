#ifndef ALGINE_BLOOMSEARCHSHADER_H
#define ALGINE_BLOOMSEARCHSHADER_H

#define constant(name, val) constexpr char name[] = val;

namespace algine::BloomSearchShader::Vars {
constant(BaseImage, "image")
constant(BrightnessThreshold, "brightnessThreshold")
}

#undef constant

#endif //ALGINE_BLOOMSEARCHSHADER_H
