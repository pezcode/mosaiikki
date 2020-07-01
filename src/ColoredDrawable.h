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

    explicit ColoredDrawable(Object3D& object,
                             Magnum::Shaders::Phong& shader,
                             Magnum::GL::Mesh& mesh,
                             const Magnum::Color4& color = Magnum::Color4(1.0f, 1.0f, 1.0f)) :
        Magnum::SceneGraph::Drawable3D(object), shader(shader), mesh(mesh), color(color)
    {
    }

    ColoredDrawable(const ColoredDrawable& other, Object3D& object) :
        Magnum::SceneGraph::Drawable3D(object),
        shader(other.shader),
        mesh(other.mesh),
        color(other.color)
    {
    }

    Magnum::GL::Mesh& getMesh() const
    {
        return mesh;
    }

    void setColor(const Magnum::Color4& newColor)
    {
        this->color = newColor;
    }

protected:
    virtual void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D& camera) override
    {
        shader.setDiffuseColor(color)
            .setTransformationMatrix(transformationMatrix)
            .setNormalMatrix(transformationMatrix.normalMatrix())
            .setProjectionMatrix(camera.projectionMatrix());

        shader.draw(mesh);
    }

    Magnum::Shaders::Phong& shader;
    Magnum::GL::Mesh& mesh;
    Magnum::Color4 color;
};
