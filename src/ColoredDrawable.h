#pragma once

#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Color.h>
#include <Magnum/SceneGraph/Camera.h>

template<typename Transform>
class ColoredDrawable : public Magnum::SceneGraph::Drawable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    explicit ColoredDrawable(Object3D& object,
                             Magnum::Shaders::Phong& shader,
                             Magnum::GL::Mesh& mesh,
                             const Magnum::Vector3& lightPos,
                             const Magnum::Color4& color) :
        Magnum::SceneGraph::Drawable3D(object), shader(shader), mesh(mesh), lightPos(lightPos), color(color)
    {
    }

    ColoredDrawable(const ColoredDrawable& other, Object3D& object) :
        Magnum::SceneGraph::Drawable3D(object),
        shader(other.shader),
        mesh(other.mesh),
        lightPos(other.lightPos),
        color(other.color)
    {
    }

    Magnum::GL::Mesh& getMesh() const
    {
        return mesh;
    }

    Magnum::Color4 getColor() const
    {
        return color;
    }
    void setColor(const Magnum::Color4& newColor)
    {
        this->color = newColor;
    }

private:
    virtual void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D& camera) override
    {
        shader.setDiffuseColor(color)
            .setLightPosition(camera.cameraMatrix().transformPoint(lightPos))
            .setTransformationMatrix(transformationMatrix)
            .setNormalMatrix(transformationMatrix.normalMatrix())
            .setProjectionMatrix(camera.projectionMatrix());

        shader.draw(mesh);
    }

    Magnum::Shaders::Phong& shader;
    Magnum::GL::Mesh& mesh;
    const Magnum::Vector3 lightPos;
    Magnum::Color4 color;
};
