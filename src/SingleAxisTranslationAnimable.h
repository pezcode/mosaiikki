#pragma once

#include <Magnum/SceneGraph/Animable.h>
#include <Magnum/SceneGraph/AbstractTranslation.h>
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/Math/Vector3.h>

// continually moves an object back and forth along an axis
// oscillates around the original position with the given velocity, range units in each direction
// animation happens relative to the current position so you can overlay translations
template<typename Transform> class SingleAxisTranslationAnimable : public Magnum::SceneGraph::Animable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    SingleAxisTranslationAnimable(Object3D& object,
                                  const Magnum::Vector3& axis,
                                  float range, // units
                                  float velocity) : // units per second
        Magnum::SceneGraph::Animable3D(object),
        transformation(object),
        axis(axis),
        range(Magnum::Math::abs(range)),
        velocity(velocity),
        distance(0.0f),
        direction(1.0f)
    {
        setRepeated(true);
    }

    SingleAxisTranslationAnimable(const SingleAxisTranslationAnimable& other, Object3D& object) :
        Magnum::SceneGraph::Animable3D(object),
        transformation(object),
        axis(other.axis),
        range(other.range),
        velocity(other.velocity),
        distance(other.distance),
        direction(other.direction)
    {
        setRepeated(true);
    }

    float getRange() const { return range; }
    void setRange(float newRange) { this->range = newRange; }

private:
    virtual void animationStopped() override
    {
        transformation.translate(axis * -distance);
        distance = 0.0f;
        direction = 1.0f;
    }

    virtual void animationStep(Magnum::Float /*absolute*/, Magnum::Float delta) override
    {
        float deltaDistance = direction * velocity * delta;
        float diff = Magnum::Math::abs(distance + deltaDistance) - range;
        while(diff > 0.0f)
        {
            // handle reflected distance, important for huge deltas
            direction *= -1.0f;
            deltaDistance += 2.0f * diff * direction;
            diff -= 2.0f * range;
        }
        distance += deltaDistance;
        transformation.translate(axis * deltaDistance);
    }

    Magnum::SceneGraph::AbstractTranslation3D& transformation;

    const Magnum::Vector3 axis;
    float range;
    const float velocity;

    float distance;
    float direction;
};
