#pragma once

#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/AbstractObject.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/Shaders/Generic.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/GrowableArray.h>

template<typename Transform>
class InstanceDrawable : public Magnum::SceneGraph::Drawable3D
{
public:
    typedef Magnum::SceneGraph::AbstractObject<Transform::Dimensions, typename Transform::Type> Object;

    struct InstanceData
    {
        Magnum::Matrix4 transformationMatrix;
        Magnum::Matrix3 normalMatrix;
        Magnum::Color4 color;
    };

    typedef Corrade::Containers::Array<InstanceData> InstanceArray;

    explicit InstanceDrawable(Object& object, InstanceArray& instanceData) :
        Magnum::SceneGraph::Drawable3D(object), instanceData(instanceData)
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
                                      Magnum::Shaders::Generic3D::TransformationMatrix(),
                                      Magnum::Shaders::Generic3D::NormalMatrix(),
                                      Magnum::Shaders::Generic3D::Color4());
        return Magnum::GL::Buffer(std::move(instanceBuffer));
    }

protected:
    virtual void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D& /* camera */) override
    {
        Corrade::Containers::arrayAppend(instanceData,
                                         { transformationMatrix, transformationMatrix.normalMatrix(), color });
    }

    Magnum::Color4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

    InstanceArray& instanceData;
};
