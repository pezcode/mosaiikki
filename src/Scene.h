#pragma once

#include "Feature.h"

#include "Drawables/ColoredDrawable.h"
#include "Drawables/ColoredDrawable.h"
#include "Drawables/TexturedDrawable.h"
#include "Drawables/TexturedDrawable.h"
#include "Drawables/VelocityDrawable.h"
#include "Drawables/VelocityDrawable.h"
#include "AxisTranslationAnimable.h"
#include "AxisRotationAnimable.h"
#include "Shaders/VelocityShader.h"
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/AnimableGroup.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Containers/Array.h>

class Scene
{
public:
    typedef Magnum::SceneGraph::MatrixTransformation3D Transform3D;
    typedef Magnum::SceneGraph::Object<Transform3D> Object3D;
    typedef Magnum::SceneGraph::Scene<Transform3D> Scene3D;

    typedef ColoredDrawable<Transform3D> ColoredDrawable3D;
    typedef TexturedDrawable<Transform3D> TexturedDrawable3D;
    typedef InstanceDrawable<Transform3D> InstanceDrawable3D;

    typedef VelocityDrawable<Scene::Transform3D> VelocityDrawable3D;
    typedef VelocityInstanceDrawable<Scene::Transform3D> VelocityInstanceDrawable3D;

    typedef AxisTranslationAnimable<Transform3D> TranslationAnimable3D;
    typedef AxisRotationAnimable<Transform3D> RotationAnimable3D;

    explicit Scene(Magnum::NoCreateT);
    explicit Scene();

    // Copying is not allowed
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Move constructor/assignment
    // TODO default functions are deleted because some(?) member is not movable
    //Scene(Scene&&) noexcept = default;
    //Scene& operator=(Scene&&) noexcept = default;

    bool loadScene(const char* file, Object3D& parent, Magnum::Range3D* bounds = nullptr);
    void addObject(Magnum::Trade::AbstractImporter& importer,
                   Magnum::UnsignedInt objectId,
                   Object3D& parent,
                   Magnum::UnsignedInt meshOffset = 0,
                   Magnum::UnsignedInt materialOffset = 0,
                   Magnum::UnsignedInt textureOffset = 0);

    // normal meshes with default instance data (transformation, normal matrix, color)
    Corrade::Containers::Array<Corrade::Containers::Pointer<Magnum::GL::Mesh>> meshes;
    Corrade::Containers::Array<Corrade::Containers::Pointer<Magnum::GL::Buffer>> instanceBuffers;

    // meshes with instance data for the velocity shader (transformation, old transformation)
    Corrade::Containers::Array<Corrade::Containers::Pointer<Magnum::GL::Mesh>> velocityMeshes;
    Corrade::Containers::Array<Corrade::Containers::Pointer<Magnum::GL::Buffer>> velocityInstanceBuffers;

    Corrade::Containers::Array<Corrade::Containers::Pointer<Magnum::Trade::PhongMaterialData>> materials;
    Corrade::Containers::Array<Corrade::Containers::Pointer<Magnum::GL::Texture2D>> textures;

    Scene3D scene;
    Object3D root;

    Object3D cameraObject;
    Corrade::Containers::Pointer<Magnum::SceneGraph::Camera3D> camera;
    float cameraNear = 1.0f;
    float cameraFar = 50.0f;

    Magnum::SceneGraph::AnimableGroup3D meshAnimables;
    Magnum::SceneGraph::AnimableGroup3D cameraAnimables;

    Magnum::SceneGraph::DrawableGroup3D drawables;
    Magnum::SceneGraph::DrawableGroup3D velocityDrawables; // moving objects that contribute to the velocity buffer

    static constexpr size_t objectGridSize = 6;
    Corrade::Containers::Array<Magnum::Vector3> lightPositions;
    Corrade::Containers::Array<Magnum::Color4> lightColors;

    Magnum::Shaders::Phong coloredMaterialShader;
    Magnum::Shaders::Phong texturedMaterialShader;
    VelocityShader velocityShader;
};
