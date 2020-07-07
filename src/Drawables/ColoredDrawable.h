#pragma once

#include "Drawables/InstanceDrawable.h"
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
class ColoredDrawable : public Magnum::SceneGraph::Drawable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    explicit ColoredDrawable(Object3D& object,
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

    InstanceDrawable<Transform>& addInstance(Object3D& object)
    {
        InstanceDrawable<Transform>* instance = new InstanceDrawable<Transform>(object, instanceData);
        instanceDrawables.add(*instance);
        return *instance;
    }

    Magnum::SceneGraph::DrawableGroup3D& instances()
    {
        return instanceDrawables;
    }

protected:
    virtual void draw(const Magnum::Matrix4& /* transformationMatrix */, Magnum::SceneGraph::Camera3D& camera) override
    {
        if(instanceDrawables.isEmpty())
            return;

        shader.setShininess(shininess).setProjectionMatrix(camera.projectionMatrix());

        Corrade::Containers::arrayResize(instanceData, 0);
        camera.draw(instanceDrawables);

        instanceBuffer.setData(instanceData, Magnum::GL::BufferUsage::DynamicDraw);
        _mesh.setInstanceCount(instanceData.size());

        shader.draw(_mesh);
    }

    Magnum::Shaders::Phong& shader;
    Magnum::UnsignedInt _meshId;
    Magnum::GL::Mesh& _mesh;
    Magnum::GL::Buffer& instanceBuffer;
    Magnum::SceneGraph::DrawableGroup3D instanceDrawables;

    float shininess;

    typename InstanceDrawable<Transform>::InstanceArray instanceData;
};
