#pragma once

#include "Shaders/VelocityShader.h"
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/GrowableArray.h>

template<typename Transform>
class VelocityInstanceDrawable : public Magnum::SceneGraph::Drawable3D
{
public:
    typedef Magnum::SceneGraph::Object<Transform> Object3D;

    struct InstanceData
    {
        Magnum::Matrix4 transformationMatrix;
        Magnum::Matrix4 oldTransformationMatrix;
    };

    typedef Corrade::Containers::Array<InstanceData> InstanceArray;

    explicit VelocityInstanceDrawable(Object3D& object, InstanceArray& instanceData) :
        Magnum::SceneGraph::Drawable3D(object),
        oldTransformation(Magnum::Math::IdentityInit),
        instanceData(instanceData)
    {
    }

    static Magnum::GL::Buffer addInstancedBuffer(Magnum::GL::Mesh& mesh)
    {
        Magnum::GL::Buffer instanceBuffer(Magnum::GL::Buffer::TargetHint::Array);
        mesh.addVertexBufferInstanced(instanceBuffer,
                                      1, // divisor
                                      0, // offset
                                      VelocityShader::TransformationMatrix(),
                                      VelocityShader::OldTransformationMatrix());
        return Magnum::GL::Buffer(std::move(instanceBuffer));
    }

protected:
    virtual void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D& /* camera */) override
    {
        Corrade::Containers::arrayAppend(instanceData, { transformationMatrix, oldTransformation });
        oldTransformation = transformationMatrix;
    }

    Magnum::Matrix4 oldTransformation;

    InstanceArray& instanceData;
};
