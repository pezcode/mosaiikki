#pragma once

#include "VelocityInstanceDrawable.h"
#include "Shaders/VelocityShader.h"
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/GL/Mesh.h>

template<typename Transform>
class VelocityDrawableInstanced : public Magnum::SceneGraph::Drawable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    explicit VelocityDrawableInstanced(Object3D& object,
                                       VelocityShader& shader,
                                       Magnum::UnsignedInt meshId,
                                       Magnum::GL::Mesh& mesh,
                                       Magnum::GL::Buffer& instanceBuffer) :
        Magnum::SceneGraph::Drawable3D(object),
        shader(shader),
        _meshId(meshId),
        _mesh(mesh),
        instanceBuffer(instanceBuffer)
    {
    }

    explicit VelocityDrawableInstanced(const VelocityDrawableInstanced& other, Object3D& object) :
        Magnum::SceneGraph::Drawable3D(object),
        shader(other.shader),
        _meshId(other._meshId),
        _mesh(other._mesh),
        instanceBuffer(other.instanceBuffer)
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

    Magnum::SceneGraph::DrawableGroup3D& instanceDrawables()
    {
        return _instanceDrawables;
    }

    typename VelocityInstanceDrawable<Transform>::InstanceArray& instanceData()
    {
        return _instanceData;
    }

private:
    virtual void draw(const Magnum::Matrix4& /* transformationMatrix */, Magnum::SceneGraph::Camera3D& camera) override
    {
        if(_instanceDrawables.isEmpty())
            return;

        Corrade::Containers::arrayResize(_instanceData, 0);
        camera.draw(_instanceDrawables);

        instanceBuffer.setData(_instanceData, GL::BufferUsage::DynamicDraw);
        _mesh.setInstanceCount(_instanceData.size());

        shader.draw(_mesh);
    }

    VelocityShader& shader;
    Magnum::UnsignedInt _meshId;
    Magnum::GL::Mesh& _mesh;
    Magnum::GL::Buffer& instanceBuffer;
    Magnum::SceneGraph::DrawableGroup3D _instanceDrawables;

    typename VelocityInstanceDrawable<Transform>::InstanceArray _instanceData;
};
