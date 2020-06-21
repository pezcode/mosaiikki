#pragma once

struct Options
{
    bool reuseVelocityDepth = true;

    struct Scene
    {
        bool animatedObjects = false;
        bool animatedCamera = false;
    } scene;

    struct Reconstruction
    {
        bool assumeOcclusion = true;

        struct Debug
        {
            enum Samples : int
            {
                All = 0,
                Even,
                Odd
            };

            Samples showSamples = All;
            bool showVelocity = false;
        } debug;
    } reconstruction;
};
