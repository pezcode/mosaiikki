#include "ColoredDrawable.h"

#include <Magnum/SceneGraph/DualQuaternionTransformation.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/SceneGraph/RigidMatrixTransformation3D.h>
#include <Magnum/SceneGraph/TranslationRotationScalingTransformation3D.h>

using namespace Magnum;

template<typename Transform>
ColoredDrawable<Transform>::ColoredDrawable(Object3D& object,
                                            Shaders::Phong& shader,
                                            GL::Mesh& mesh,
                                            const Magnum::Vector3& lightPos,
                                            const Color4& color,
                                            SceneGraph::DrawableGroup3D& group) :
    SceneGraph::Drawable3D(object, &group), shader(shader), mesh(mesh), lightPos(lightPos), color(color)
{

}

template<typename Transform>
void ColoredDrawable<Transform>::draw(const Matrix4& transformationMatrix, SceneGraph::Camera3D& camera)
{
    shader
        .setDiffuseColor(color)
        .setLightPosition(camera.cameraMatrix().transformPoint(lightPos))
        .setTransformationMatrix(transformationMatrix)
        .setNormalMatrix(transformationMatrix.normalMatrix())
        .setProjectionMatrix(camera.projectionMatrix());

    mesh.draw(shader);
}

// need to explicitly instantiate these so the compiler can find them
template class ColoredDrawable<SceneGraph::DualQuaternionTransformation>;
template class ColoredDrawable<SceneGraph::TranslationRotationScalingTransformation3D>;
template class ColoredDrawable<SceneGraph::MatrixTransformation3D>;
template class ColoredDrawable<SceneGraph::RigidMatrixTransformation3D>;
