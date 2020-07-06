#pragma once

#include "Shaders/VelocityShader.h"
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/GL/Mesh.h>

template<typename Transform>
class VelocityDrawable : public Magnum::SceneGraph::Drawable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    explicit VelocityDrawable(Object3D& object, VelocityShader& shader, Magnum::GL::Mesh& mesh) :
        Magnum::SceneGraph::Drawable3D(object),
        shader(shader),
        _mesh(mesh),
        oldTransformation(Magnum::Math::IdentityInit)
    {
    }

    explicit VelocityDrawable(const VelocityDrawable& other, Object3D& object) :
        Magnum::SceneGraph::Drawable3D(object),
        shader(other.shader),
        _mesh(other._mesh),
        oldTransformation(other.oldTransformation)
    {
    }

    Magnum::GL::Mesh& mesh() const
    {
        return _mesh;
    }

private:
    virtual void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D& /*camera*/) override
    {
        shader.setOldTransformationMatrix(oldTransformation);
        shader.setTransformationMatrix(transformationMatrix);
        oldTransformation = transformationMatrix;

        shader.draw(_mesh);
    }

    VelocityShader& shader;
    Magnum::GL::Mesh& _mesh;
    Magnum::Matrix4 oldTransformation;
};
