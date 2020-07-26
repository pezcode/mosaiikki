#pragma once

#include <Magnum/SceneGraph/Animable.h>
#include <Magnum/SceneGraph/AbstractTranslationRotation3D.h>
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Angle.h>
#include <Magnum/Math/Constants.h>

// continually rotate an object around an axis with a given angular velocity
// rotates until a given angle (range) has been covered, then rotates in the opposite direction
template<typename Transform>
class AxisRotationAnimable : public Magnum::SceneGraph::Animable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    explicit AxisRotationAnimable(Object3D& object,
                                  const Magnum::Vector3& axis,
                                  Magnum::Rad velocity, /* radians per second */
                                  Magnum::Rad range = Magnum::Rad(Magnum::Constants::inf()) /* radians */) :
        Magnum::SceneGraph::Animable3D(object),
        transformation(object),
        axis(axis.normalized()),
        velocity(velocity),
        range(Magnum::Math::abs(range))
    {
        setRepeated(true);
    }

private:
    virtual void animationStopped() override
    {
        transformation.rotateLocal(-distance, axis);
        distance = Magnum::Rad(0.0f);
        direction = 1.0f;
    }

    virtual void animationStep(Magnum::Float /*absolute*/, Magnum::Float delta) override
    {
        Magnum::Rad deltaDistance = direction * velocity * delta;
        Magnum::Rad diff = Magnum::Math::abs(distance + deltaDistance) - range;
        while(diff > Magnum::Rad(0.0f))
        {
            direction = -direction;
            deltaDistance += 2.0f * diff * direction;
            diff -= 2.0f * range;
        }

        distance += deltaDistance;
        transformation.rotateLocal(deltaDistance, axis);
    }

    Magnum::SceneGraph::AbstractTranslationRotation3D& transformation;

    const Magnum::Vector3 axis;
    const Magnum::Rad velocity;
    const Magnum::Rad range;

    Magnum::Rad distance = Magnum::Rad(0.0f);
    Magnum::Float direction = 1.0f;
};
