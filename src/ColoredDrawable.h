#pragma once

#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Color.h>

template<typename Transform>
class ColoredDrawable : public Magnum::SceneGraph::Drawable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    explicit ColoredDrawable(Object3D& object, Magnum::Shaders::Phong& shader, Magnum::GL::Mesh& mesh) :
        Magnum::SceneGraph::Drawable3D(object), shader(shader), _mesh(mesh), color(1.0f, 1.0f, 1.0f), shininess(80.0f)
    {
    }

    Magnum::GL::Mesh& mesh() const
    {
        return _mesh;
    }

    void setColor(const Magnum::Color4& newColor)
    {
        color = newColor;
    }

    void setShininess(float newShininess)
    {
        shininess = newShininess;
    }

protected:
    virtual void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D& camera) override
    {
        shader.setDiffuseColor(color)
            .setShininess(shininess)
            .setTransformationMatrix(transformationMatrix)
            .setNormalMatrix(transformationMatrix.normalMatrix())
            .setProjectionMatrix(camera.projectionMatrix());

        shader.draw(_mesh);
    }

    Magnum::Shaders::Phong& shader;
    Magnum::GL::Mesh& _mesh;
    Magnum::Color4 color;
    float shininess;
};
