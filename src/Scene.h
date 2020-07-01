#pragma once

#include "Feature.h"

#include "ColoredDrawable.h"
#include "TexturedDrawable.h"
#include "SingleAxisTranslationAnimable.h"
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/AnimableGroup.h>
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Texture.h>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>

class Scene
{
public:

    typedef Magnum::SceneGraph::MatrixTransformation3D Transform3D;
    typedef Magnum::SceneGraph::Object<Transform3D> Object3D;
    typedef Magnum::SceneGraph::Scene<Transform3D> Scene3D;
    
    typedef ColoredDrawable<Transform3D> ColoredDrawable3D;
    typedef TexturedDrawable<Transform3D> TexturedDrawable3D;
    typedef SingleAxisTranslationAnimable<Transform3D> Animable3D;

    explicit Scene(Magnum::NoCreateT);
    explicit Scene();

    // Copying is not allowed
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Move constructor/assignment
    // TODO default functions are deleted because some(?) member is not movable
    //Scene(Scene&&) noexcept = default;
    //Scene& operator=(Scene&&) noexcept = default;

    bool loadScene(const char* file, Object3D& parent);
    void addObject(Magnum::Trade::AbstractImporter& importer,
                   Magnum::UnsignedInt objectId,
                   Object3D& parent,
                   Magnum::UnsignedInt textureOffset = 0);
    Object3D& duplicateObject(Object3D& object, Object3D& parent);

    Corrade::Containers::Array<Corrade::Containers::Optional<Magnum::GL::Mesh>> meshes;
    Corrade::Containers::Array<Corrade::Containers::Optional<Magnum::GL::Texture2D>> textures;
    Corrade::Containers::Array<Corrade::Containers::Optional<Magnum::Trade::PhongMaterialData>> materials;

    Scene3D scene;
    Object3D root, cameraObject;
    Corrade::Containers::Pointer<Magnum::SceneGraph::Camera3D> camera;
    Magnum::SceneGraph::DrawableGroup3D drawables;

    float cameraNear = 1.0f;
    float cameraFar = 50.0f;

    Magnum::SceneGraph::AnimableGroup3D meshAnimables;
    Magnum::SceneGraph::AnimableGroup3D cameraAnimables;
    static constexpr size_t objectGridSize = 6;
    Corrade::Containers::Array<Magnum::Vector3> lightPositions;
    Corrade::Containers::Array<Magnum::Color4> lightColors;

    Magnum::Shaders::Phong coloredMaterialShader;
    Magnum::Shaders::Phong texturedMaterialShader;
};
