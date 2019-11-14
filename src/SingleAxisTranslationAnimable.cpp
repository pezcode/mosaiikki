#include "SingleAxisTranslationAnimable.h"

#include <Magnum/SceneGraph/DualQuaternionTransformation.h>
#include <Magnum/SceneGraph/TranslationRotationScalingTransformation3D.h>
#include <Magnum/SceneGraph/TranslationTransformation.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/SceneGraph/RigidMatrixTransformation3D.h>

using namespace Magnum;
using namespace Magnum::Math::Literals;

template<typename Transform>
SingleAxisTranslationAnimable<Transform>::SingleAxisTranslationAnimable(Object3D& object,
                                                                        const Magnum::Vector3& axis,
                                                                        float range,
                                                                        float velocity) :
    SceneGraph::Animable3D(object),
    object(object),
    axis(axis),
    range(Math::abs(range)),
    velocity(velocity),
    distance(0.0f),
    direction(1.0f)
{
    setRepeated(true);
}

template<typename Transform>
SingleAxisTranslationAnimable<Transform>::SingleAxisTranslationAnimable(const SingleAxisTranslationAnimable& other, Object3D& object) :
    SceneGraph::Animable3D(object),
    object(object),
    axis(other.axis),
    range(other.range),
    velocity(other.velocity),
    distance(other.distance),
    direction(other.direction)
{
    setRepeated(true);
}

template<typename Transform>
void SingleAxisTranslationAnimable<Transform>::animationStopped()
{
    object.translate(axis * -distance);
    distance = 0.0f;
    direction = 1.0f;
}

template<typename Transform>
void SingleAxisTranslationAnimable<Transform>::animationStep(Float /*absolute*/, Float delta)
{
    float deltaDistance = direction * velocity * delta;
    float diff = Math::abs(distance + deltaDistance) - range;
    while(diff > 0.0f)
    {
        // handle reflected distance, important for huge deltas
        direction *= -1.0f;
        deltaDistance += 2.0f * diff * direction;
        diff -= 2.0f * range;
    }
    distance += deltaDistance;
    object.translate(axis * deltaDistance);
}

template class SingleAxisTranslationAnimable<SceneGraph::DualQuaternionTransformation>;
template class SingleAxisTranslationAnimable<SceneGraph::TranslationRotationScalingTransformation3D>;
template class SingleAxisTranslationAnimable<SceneGraph::TranslationTransformation3D>;
template class SingleAxisTranslationAnimable<SceneGraph::MatrixTransformation3D>;
template class SingleAxisTranslationAnimable<SceneGraph::RigidMatrixTransformation3D>;
