#pragma once

#include <Magnum/SceneGraph/Object.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/GL/Buffer.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/GrowableArray.h>

template<typename Transform>
class InstanceDrawable : public Magnum::SceneGraph::Drawable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    struct InstanceData
    {
        Magnum::Matrix4 transformationMatrix;
        Magnum::Matrix3 normalMatrix;
        Magnum::Color4 color;
    };

    explicit InstanceDrawable(Object3D& object, Corrade::Containers::Array<InstanceData>& instanceData) :
        Magnum::SceneGraph::Drawable3D(object), instanceData(instanceData)
    {
    }

    InstanceDrawable(const InstanceDrawable& other, Object3D& object) :
        Magnum::SceneGraph::Drawable3D(object), color(other.color), instanceData(other.instanceData)
    {
    }

    void setColor(const Magnum::Color4& newColor)
    {
        color = newColor;
    }

    static Magnum::GL::Buffer addInstancedBuffer(Magnum::GL::Mesh& mesh)
    {
        Magnum::GL::Buffer instanceBuffer(Magnum::GL::Buffer::TargetHint::Array);
        mesh.addVertexBufferInstanced(instanceBuffer,
                                      1, // divisor
                                      0, // offset
                                      Magnum::Shaders::Phong::TransformationMatrix(),
                                      Magnum::Shaders::Phong::NormalMatrix(),
                                      Magnum::Shaders::Phong::Color4());
        return std::move(instanceBuffer);
    }

protected:
    virtual void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D& /* camera */) override
    {
        Corrade::Containers::arrayAppend(instanceData,
                                         { transformationMatrix, transformationMatrix.normalMatrix(), color });
    }

    Magnum::Color4 color;
    Corrade::Containers::Array<InstanceData>& instanceData;
};
