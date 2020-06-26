#include "MagnumShadersSampleInterpolationOverride.h"

#include <Corrade/Utility/Resource.h>
#include <Corrade/Utility/Directory.h>
#include <Corrade/Utility/FormatStl.h>
#include <Corrade/Utility/String.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Extensions.h>
#include <ctime>
#include <cstdlib>

using namespace Corrade::Utility;

const std::string MagnumShadersSampleInterpolationOverride::resourceGroup = "MagnumShaders";

MagnumShadersSampleInterpolationOverride::MagnumShadersSampleInterpolationOverride(std::initializer_list<std::string> list)
{
    if(!Resource::hasGroup(resourceGroup))
        return;

    if(!Magnum::GL::Context::current().isExtensionSupported<Magnum::GL::Extensions::ARB::gpu_shader5>())
    {
        Warning() << "Sample interpolation not supported, requires" << Magnum::GL::Version::GL400 << "or ARB_gpu_shader5";
        return;
    }

    Resource rs(resourceGroup);

    std::srand(std::time(nullptr));
    tmpDirectory = Directory::join({ Directory::tmp(), formatString("{}_override_{}", resourceGroup, std::abs(std::rand())) });
    Directory::mkpath(tmpDirectory);

    std::string configFile = Directory::join(tmpDirectory, "override.conf");

    Directory::writeString(configFile, "group=" + resourceGroup);

    bool success = true;
    for(const std::string& shader : list)
    {
        Directory::appendString(configFile, "\n\n[file]\nfilename=" + shader);
        const std::string src = rs.get(shader);

        std::string patched = "#extension GL_ARB_gpu_shader5: require\n";
        const std::string extension = Directory::splitExtension(shader).second;
        if(extension == ".vert")
            patched += String::replaceAll(src, "\nout ", "\nsample out ");
        else if(extension == ".frag")
            patched += String::replaceAll(src, "\nin ", "\nsample in ");
        else
            patched = src;

        success = success && Directory::writeString(Directory::join(tmpDirectory, shader), patched);
    }

    if(success)
    {
        Resource::overrideGroup(resourceGroup, configFile);
    }
}

MagnumShadersSampleInterpolationOverride::~MagnumShadersSampleInterpolationOverride()
{
    // empty configuration file discards override
    if(Resource::hasGroup(resourceGroup))
        Resource::overrideGroup(resourceGroup, "");

    if(!tmpDirectory.empty())
    {
        for(const std::string& path : Directory::list(tmpDirectory, Directory::Flag::SkipDirectories | Directory::Flag::SkipSpecial))
            Directory::rm(Directory::join(tmpDirectory, path));
        Directory::rm(tmpDirectory);
    }
}
