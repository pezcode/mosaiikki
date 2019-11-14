#pragma once

#include <Magnum/SceneGraph/Animable.h>
#include <Magnum/Math/Vector3.h>

template<typename Transform> class SingleAxisTranslationAnimable : public Magnum::SceneGraph::Animable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    SingleAxisTranslationAnimable(Object3D& object,
        const Magnum::Vector3& axis,
        float range, // units
        float velocity); // units per second

private:
    virtual void animationStarted() override;
    virtual void animationStopped() override;
    virtual void animationStep(Magnum::Float absolute, Magnum::Float delta) override;

    Object3D& object;
    const Magnum::Vector3 axis;
    const float range;
    const float velocity;

    float distance;
    float direction;
    typename Transform::DataType initialTransform;
};
