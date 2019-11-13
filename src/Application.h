#pragma once

#include "ImGuiApplication.h"
#include "ColoredDrawable.h"
#include "SingleAxisTranslationAnimable.h"
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/AnimableGroup.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/Timeline.h>
#include <Magnum/GL/GL.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/Math/Color.h>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>

class Application : public ImGuiApplication
{
public:
    explicit Application(const Arguments& arguments);

private:
    typedef Magnum::SceneGraph::MatrixTransformation3D Transform3D;
    typedef Magnum::SceneGraph::Object<Transform3D> Object3D;
    typedef Magnum::SceneGraph::Scene<Transform3D> Scene3D;
    typedef ColoredDrawable<Transform3D> Drawable;
    typedef SingleAxisTranslationAnimable<Transform3D> Animable;

    virtual void drawEvent() override;
    virtual void viewportEvent(ViewportEvent& event) override;
    virtual void buildUI() override;

    bool loadScene(const char* file, Object3D& parent);

    // scene rendering

    Corrade::Containers::Array<Corrade::Containers::Optional<Magnum::GL::Mesh>> meshes;

    Scene3D scene;
    Object3D manipulator, cameraObject;
    Corrade::Containers::Pointer<Magnum::SceneGraph::Camera3D> camera;
    Magnum::SceneGraph::DrawableGroup3D drawables;
    Magnum::SceneGraph::AnimableGroup3D animables;
    Magnum::SceneGraph::AnimableGroup3D cameraAnimable;

    Magnum::Timeline timeline;

    Magnum::Shaders::Phong meshShader;
    Magnum::Vector3 lightPos;

    // checkerboard rendering

    // checkerboard framebuffers
    // quarter size (half width, half height)
    Magnum::GL::Framebuffer framebuffers[2];
    Magnum::GL::Texture2D colorAttachments[2];
    Magnum::GL::Renderbuffer depthStencilAttachments[2];

    size_t currentFramebuffer;
};
