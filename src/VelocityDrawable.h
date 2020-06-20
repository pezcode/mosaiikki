#pragma once

#include "Shaders/VelocityShader.h"
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/SceneGraph/Camera.h>

template<typename Transform>
class VelocityDrawable : public Magnum::SceneGraph::Drawable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    explicit VelocityDrawable(Object3D& object, VelocityShader& shader, Magnum::GL::Mesh& mesh) :
        Magnum::SceneGraph::Drawable3D(object),
        shader(shader),
        mesh(mesh),
        oldTransformation(Magnum::Math::IdentityInit)
    {
    }

    VelocityDrawable(const VelocityDrawable& other, Object3D& object) :
        Magnum::SceneGraph::Drawable3D(object),
        shader(other.shader),
        mesh(other.mesh),
        oldTransformation(other.oldTransformation)
    {
    }

    Magnum::GL::Mesh& getMesh() const
    {
        return mesh;
    }

private:
    virtual void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D& /*camera*/) override
    {
        shader.setOldTransformation(oldTransformation);
        shader.setTransformation(transformationMatrix);
        oldTransformation = transformationMatrix;

        shader.draw(mesh);
    }

    VelocityShader& shader;
    Magnum::GL::Mesh& mesh;
    Magnum::Matrix4 oldTransformation;
};
