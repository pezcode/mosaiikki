#include "SingleAxisAnimable.h"

#include <Magnum/SceneGraph/DualQuaternionTransformation.h>
#include <Magnum/SceneGraph/TranslationRotationScalingTransformation3D.h>
#include <Magnum/SceneGraph/TranslationTransformation.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/SceneGraph/RigidMatrixTransformation3D.h>
#include <Magnum/Math/Angle.h>

using namespace Magnum;
using namespace Magnum::Math::Literals;

template<typename Transform>
SingleAxisAnimable<Transform>::SingleAxisAnimable(Object3D& object) :
    SceneGraph::Animable3D(object),
    object(object)
{
    setRepeated(true);
}

template<typename Transform>
void SingleAxisAnimable<Transform>::animationStep(Float absolute, Float delta)
{
    object.rotateY(15.0_degf * delta);
}

template class SingleAxisAnimable<SceneGraph::DualQuaternionTransformation>;
template class SingleAxisAnimable<SceneGraph::TranslationRotationScalingTransformation3D>;
//template class SingleAxisAnimable<SceneGraph::TranslationTransformation3D>;
template class SingleAxisAnimable<SceneGraph::MatrixTransformation3D>;
template class SingleAxisAnimable<SceneGraph::RigidMatrixTransformation3D>;
