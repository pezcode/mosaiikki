#pragma once

#include <initializer_list>
#include <string>

class MagnumShadersSampleInterpolationOverride
{
public:
    MagnumShadersSampleInterpolationOverride(std::initializer_list<std::string> list);
    ~MagnumShadersSampleInterpolationOverride();

private:
    static const std::string resourceGroup;
    std::string tmpDirectory;
};
