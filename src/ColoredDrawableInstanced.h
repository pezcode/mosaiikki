#pragma once

#include "InstanceDrawable.h"
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/Math/Color.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/GrowableArray.h>

template<typename Transform>
class ColoredDrawableInstanced : public Magnum::SceneGraph::Drawable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;
    typedef Corrade::Containers::Array<typename InstanceDrawable<Transform>::InstanceData> InstanceArray;

    explicit ColoredDrawableInstanced(Object3D& object,
                                      Magnum::Shaders::Phong& shader,
                                      Magnum::GL::Mesh& mesh,
                                      Magnum::GL::Buffer& instanceBuffer) :
        Magnum::SceneGraph::Drawable3D(object),
        shader(shader),
        mesh(mesh),
        instanceBuffer(instanceBuffer),
        shininess(80.0f)
    {
    }

    ColoredDrawableInstanced(const ColoredDrawableInstanced& other, Object3D& object) :
        Magnum::SceneGraph::Drawable3D(object),
        shader(other.shader),
        mesh(other.mesh),
        instanceBuffer(other.instanceBuffer),
        shininess(other.shininess)
    {
    }

    Magnum::GL::Mesh& getMesh() const
    {
        return mesh;
    }

    void setShininess(float newShininess)
    {
        shininess = newShininess;
    }

    Magnum::SceneGraph::DrawableGroup3D& getInstanceDrawables()
    {
        return instanceDrawables;
    }

    InstanceArray& getInstanceData()
    {
        return instanceData;
    }

protected:
    virtual void draw(const Magnum::Matrix4& /* transformationMatrix */, Magnum::SceneGraph::Camera3D& camera) override
    {
        if(instanceDrawables.isEmpty())
            return;

        shader.setShininess(shininess).setProjectionMatrix(camera.projectionMatrix());

        Corrade::Containers::arrayResize(instanceData, 0);
        camera.draw(instanceDrawables);

        instanceBuffer.setData(instanceData, GL::BufferUsage::DynamicDraw);
        mesh.setInstanceCount(instanceData.size());

        shader.draw(mesh);
    }

    Magnum::Shaders::Phong& shader;
    Magnum::GL::Mesh& mesh;
    Magnum::GL::Buffer& instanceBuffer;
    Magnum::SceneGraph::DrawableGroup3D instanceDrawables;

    float shininess;

    InstanceArray instanceData;
};
