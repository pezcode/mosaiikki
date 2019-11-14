#pragma once

#include <Magnum/SceneGraph/Animable.h>
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
        float velocity); // units per second

    SingleAxisTranslationAnimable(const SingleAxisTranslationAnimable& other, Object3D& object);

    float getRange() const { return range; }
    void setRange(float newRange) { this->range = newRange; }

private:
    virtual void animationStopped() override;
    virtual void animationStep(Magnum::Float absolute, Magnum::Float delta) override;

    Object3D& object;
    const Magnum::Vector3 axis;
    float range;
    const float velocity;

    float distance;
    float direction;
};
