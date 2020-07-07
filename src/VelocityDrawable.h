#pragma once

#include "VelocityInstanceDrawable.h"
#include "Shaders/VelocityShader.h"
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/GL/Mesh.h>

template<typename Transform>
class VelocityDrawable : public Magnum::SceneGraph::Drawable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    explicit VelocityDrawable(Object3D& object,
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

    Magnum::UnsignedInt meshId() const
    {
        return _meshId;
    }

    Magnum::GL::Mesh& mesh()
    {
        return _mesh;
    }

    VelocityInstanceDrawable<Transform>& addInstance(Object3D& object)
    {
        VelocityInstanceDrawable<Transform>* instance = new VelocityInstanceDrawable<Transform>(object, instanceData);
        instanceDrawables.add(*instance);
        return *instance;
    }

    Magnum::SceneGraph::DrawableGroup3D& instances()
    {
        return instanceDrawables;
    }

private:
    virtual void draw(const Magnum::Matrix4& /* transformationMatrix */, Magnum::SceneGraph::Camera3D& camera) override
    {
        if(instanceDrawables.isEmpty())
            return;

        Corrade::Containers::arrayResize(instanceData, 0);
        camera.draw(instanceDrawables);

        instanceBuffer.setData(instanceData, Magnum::GL::BufferUsage::DynamicDraw);
        _mesh.setInstanceCount(instanceData.size());

        shader.draw(_mesh);
    }

    VelocityShader& shader;
    Magnum::UnsignedInt _meshId;
    Magnum::GL::Mesh& _mesh;
    Magnum::GL::Buffer& instanceBuffer;
    Magnum::SceneGraph::DrawableGroup3D instanceDrawables;

    typename VelocityInstanceDrawable<Transform>::InstanceArray instanceData;
};
