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

    explicit ColoredDrawableInstanced(Object3D& object,
                                      Magnum::Shaders::Phong& shader,
                                      Magnum::UnsignedInt meshId,
                                      Magnum::GL::Mesh& mesh,
                                      Magnum::GL::Buffer& instanceBuffer) :
        Magnum::SceneGraph::Drawable3D(object),
        shader(shader),
        _meshId(meshId),
        _mesh(mesh),
        instanceBuffer(instanceBuffer),
        shininess(80.0f)
    {
    }

    explicit ColoredDrawableInstanced(const ColoredDrawableInstanced& other, Object3D& object) :
        Magnum::SceneGraph::Drawable3D(object),
        shader(other.shader),
        _meshId(other._meshId),
        _mesh(other._mesh),
        instanceBuffer(other.instanceBuffer),
        shininess(other.shininess)
    {
    }

    Magnum::UnsignedInt meshId() const
    {
        return _meshId;
    }

    Magnum::GL::Mesh& mesh()
    {
        return _mesh;
    }

    void setShininess(float newShininess)
    {
        shininess = newShininess;
    }

    Magnum::SceneGraph::DrawableGroup3D& instanceDrawables()
    {
        return _instanceDrawables;
    }

    typename InstanceDrawable<Transform>::InstanceArray& instanceData()
    {
        return _instanceData;
    }

protected:
    virtual void draw(const Magnum::Matrix4& /* transformationMatrix */, Magnum::SceneGraph::Camera3D& camera) override
    {
        if(_instanceDrawables.isEmpty())
            return;

        shader.setShininess(shininess).setProjectionMatrix(camera.projectionMatrix());

        Corrade::Containers::arrayResize(_instanceData, 0);
        camera.draw(_instanceDrawables);

        instanceBuffer.setData(_instanceData, GL::BufferUsage::DynamicDraw);
        _mesh.setInstanceCount(_instanceData.size());

        shader.draw(_mesh);
    }

    Magnum::Shaders::Phong& shader;
    Magnum::UnsignedInt _meshId;
    Magnum::GL::Mesh& _mesh;
    Magnum::GL::Buffer& instanceBuffer;
    Magnum::SceneGraph::DrawableGroup3D _instanceDrawables;

    float shininess;

    typename InstanceDrawable<Transform>::InstanceArray _instanceData;
};
