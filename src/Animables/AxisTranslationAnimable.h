#pragma once

#include <Magnum/SceneGraph/Animable.h>
#include <Magnum/SceneGraph/AbstractTranslation.h>
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Constants.h>

// continually moves an object back and forth along an axis
// oscillates around the original position with the given velocity, range units in each direction
// animation happens relative to the current position so you can overlay translations
template<typename Transform>
class AxisTranslationAnimable : public Magnum::SceneGraph::Animable3D
{
public:
    typedef Magnum::SceneGraph::AbstractObject<Transform::Dimensions, typename Transform::Type> Object;

    explicit AxisTranslationAnimable(Object& object,
                                     const Magnum::Vector3& axis,
                                     Magnum::Float velocity, /* units per second */
                                     Magnum::Float range = Magnum::Constants::inf() /* units */) :
        Magnum::SceneGraph::Animable3D(object),
        transformation(static_cast<Magnum::SceneGraph::Object<Transform>&>(object)),
        axis(axis.normalized()),
        velocity(velocity),
        range(Magnum::Math::abs(range))
    {
        setRepeated(true);
    }

private:
    virtual void animationStopped() override
    {
        transformation.translate(axis * -distance);
        distance = 0.0f;
        direction = 1.0f;
    }

    virtual void animationStep(Magnum::Float /*absolute*/, Magnum::Float delta) override
    {
        Magnum::Float deltaDistance = direction * velocity * delta;
        Magnum::Float diff = Magnum::Math::abs(distance + deltaDistance) - range;
        while(diff > 0.0f)
        {
            // handle reflected distance, important for huge deltas
            direction = -direction;
            deltaDistance += 2.0f * diff * direction;
            diff -= 2.0f * range;
        }
        distance += deltaDistance;
        transformation.translate(axis * deltaDistance);
    }

    Magnum::SceneGraph::AbstractTranslation3D& transformation;

    const Magnum::Vector3 axis;
    const Magnum::Float velocity;
    const Magnum::Float range;

    Magnum::Float distance = 0.0f;
    Magnum::Float direction = 1.0f;
};
