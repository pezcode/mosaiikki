#pragma once

#include <Magnum/Magnum.h>
#include <Magnum/SceneGraph/Animable.h>

template<typename Transform> class SingleAxisAnimable : public Magnum::SceneGraph::Animable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    SingleAxisAnimable(Object3D& object);

private:
    virtual void animationStep(Magnum::Float absolute, Magnum::Float delta) override;

    Object3D& object;
};
