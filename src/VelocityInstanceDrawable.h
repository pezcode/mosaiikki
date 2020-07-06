#pragma once

#include "Shaders/VelocityShader.h"
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/SceneGraph/Drawable.h>
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

    explicit VelocityInstanceDrawable(const VelocityInstanceDrawable& other, Object3D& object) :
        Magnum::SceneGraph::Drawable3D(object), color(other.color), instanceData(other.instanceData)
    {
    }

    static Magnum::GL::Buffer addInstancedBuffer(Magnum::GL::Mesh& mesh)
    {
        // TODO this is fucked
        // if we add a second buffer, how do the attribute locations overlap?
        // we also have to keep a second array of instance buffers around :(
        // can we reuse the generic instance buffer in InstanceDrawable?
        // just have to add OldTransformationMatrix and keep the old transformation around
        // the shader can just take whatever data they need from the instance buffer

        // good alternative:
        // compile different GL::Mesh with different instanced buffers attached
        // MeshTools::compile takes a reference to vertex buffer GL::Buffer
        // https://doc.magnum.graphics/magnum/namespaceMagnum_1_1MeshTools.html#a283d90bc9707055e65d8b61684e532d8
        // the same base vertex buffer is reused, but we can have a GL::Mesh for velocity only
        // can we somehow do this after loading the scene? we'd have to keep MeshData around

        Magnum::GL::Buffer instanceBuffer(Magnum::GL::Buffer::TargetHint::Array);
        mesh.addVertexBufferInstanced(instanceBuffer,
                                      1, // divisor
                                      0, // offset
                                      VelocityShader::TransformationMatrix(),
                                      VelocityShader::OldTransformationMatrix());
        return std::move(instanceBuffer);
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
