#pragma once

#include <Magnum/SceneGraph/Animable.h>
#include <Magnum/SceneGraph/AbstractTranslationRotation3D.h>
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Angle.h>
#include <Magnum/Math/Constants.h>
#include <Corrade/Utility/StlMath.h> // cmath but without unneeded C++ 17 stuff

// continually rotate an object around an axis with a given angular velocity
template<typename Transform>
class AxisRotationAnimable : public Magnum::SceneGraph::Animable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    explicit AxisRotationAnimable(Object3D& object,
                                  const Magnum::Vector3& axis,
                                  Magnum::Rad velocity /* radians per second */) :
        Magnum::SceneGraph::Animable3D(object),
        transformation(object),
        axis(axis.normalized()),
        velocity(velocity),
        distance(0.0f)
    {
        setRepeated(true);
    }

    explicit AxisRotationAnimable(const AxisRotationAnimable& other, Object3D& object) :
        Magnum::SceneGraph::Animable3D(object),
        transformation(object),
        axis(other.axis),
        velocity(other.velocity),
        distance(other.distance)
    {
        setRepeated(true);
    }

private:
    virtual void animationStopped() override
    {
        transformation.rotateLocal(-distance, axis);
        distance = Magnum::Rad(0.0f);
    }

    virtual void animationStep(Magnum::Float /*absolute*/, Magnum::Float delta) override
    {
        Magnum::Rad deltaDistance = velocity * delta;
        distance = Magnum::Rad(std::fmod(float(distance + deltaDistance), Magnum::Math::Constants<float>::pi()));
        transformation.rotateLocal(deltaDistance, axis);
    }

    Magnum::SceneGraph::AbstractTranslationRotation3D& transformation;

    const Magnum::Vector3 axis;
    const Magnum::Rad velocity;

    Magnum::Rad angle;
    Magnum::Rad distance;
};
