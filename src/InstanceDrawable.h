#pragma once

#include <Magnum/SceneGraph/Object.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/Shaders/Generic.h>
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
        //Magnum::Matrix4 oldTransformationMatrix;
        Magnum::Matrix3 normalMatrix;
        Magnum::Color4 color;
    };

    typedef Corrade::Containers::Array<InstanceData> InstanceArray;

    explicit InstanceDrawable(Object3D& object, InstanceArray& instanceData) :
        Magnum::SceneGraph::Drawable3D(object), instanceData(instanceData), color(1.0f, 1.0f, 1.0f) //,
    //oldTransformationMatrix(Magnum::Math::IdentityInit)
    {
    }

    explicit InstanceDrawable(const InstanceDrawable& other, Object3D& object) :
        Magnum::SceneGraph::Drawable3D(object),
        color(other.color),
        //oldTransformationMatrix(other.oldTransformationMatrix),
        instanceData(other.instanceData)
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
                                      // TODO find attribute position for old transformation
                                      // no contiguous space for one matrix
                                      // split it? only 4, 6, 7, 15 are free
                                      Magnum::Shaders::Generic3D::NormalMatrix(),
                                      Magnum::Shaders::Generic3D::Color4());
        return std::move(instanceBuffer);
    }

protected:
    virtual void draw(const Magnum::Matrix4& transformationMatrix, Magnum::SceneGraph::Camera3D& /* camera */) override
    {
        Corrade::Containers::arrayAppend(
            instanceData,
            { transformationMatrix, /*oldTransformationMatrix,*/ transformationMatrix.normalMatrix(), color });
        //oldTransformationMatrix = transformationMatrix;
    }

    Magnum::Color4 color;
    //Magnum::Matrix4 oldTransformationMatrix;

    InstanceArray& instanceData;
};
