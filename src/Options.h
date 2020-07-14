#pragma once

struct Options
{
    bool reuseVelocityDepth = true; // depends on createVelocityBuffer

    struct Scene
    {
        bool animatedObjects = false;
        bool animatedCamera = false;
    } scene;

    struct Reconstruction
    {
        bool createVelocityBuffer = true;
        bool assumeOcclusion = false;
        float depthTolerance = 0.01f;
        bool differentialBlending = true;

        struct Debug
        {
            enum Samples : int
            {
                Combined = 0,
                Even,
                Odd
            };

            Samples showSamples = Combined;
            bool showVelocity = false;
            bool showColors = false;
        } debug;
    } reconstruction;
};
