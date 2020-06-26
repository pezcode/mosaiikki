#pragma once

#include "Options.h"
#include "ImGuiApplication.h"
#include "VelocityDrawable.h"
#include "ColoredDrawable.h"
#include "SingleAxisTranslationAnimable.h"
#include "Shaders/ReconstructionShader.h"
#include "Shaders/VelocityShader.h"
#include "Shaders/DepthBlitShader.h"
#include <Magnum/SceneGraph/MatrixTransformation3D.h>
#include <Magnum/SceneGraph/Object.h>
#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/AnimableGroup.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Shaders/Phong.h>
#include <Magnum/Timeline.h>
#include <Magnum/GL/GL.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/MultisampleTexture.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/DebugTools/FrameProfiler.h>
#include <Magnum/Math/Color.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/Optional.h>
#include <fstream>

namespace Magnum
{
namespace Trade
{
class AbstractImporter;
}
} // namespace Magnum

class Mosaiikki : public ImGuiApplication
{
public:
    explicit Mosaiikki(const Arguments& arguments);

    static constexpr char const * NAME = "mosaiikki";

private:
    typedef Magnum::SceneGraph::MatrixTransformation3D Transform3D;
    typedef Magnum::SceneGraph::Object<Transform3D> Object3D;
    typedef Magnum::SceneGraph::Scene<Transform3D> Scene3D;
    typedef VelocityDrawable<Transform3D> VelocityDrawable3D;
    typedef ColoredDrawable<Transform3D> ColoredDrawable3D;
    typedef SingleAxisTranslationAnimable<Transform3D> Animable3D;

    virtual void drawEvent() override;
    virtual void viewportEvent(ViewportEvent& event) override;
    virtual void buildUI() override;

    bool loadScene(const char* file, Object3D& parent);
    void addObject(Magnum::Trade::AbstractImporter& importer, Magnum::UnsignedInt objectId, Object3D& parent);
    Object3D& duplicateObject(Object3D& object, Object3D& parent);

    static void updateProjectionMatrix(Magnum::SceneGraph::Camera3D& camera);
    void resizeFramebuffers(Magnum::Vector2i frameBufferSize);

    // debug output

    std::fstream logFile;
    // global instances to override the ostream for all instances created later
    // we need to override each of them separately
    // don't use these, they're nullptr if the file can't be created
    Corrade::Containers::Pointer<Corrade::Utility::Debug> _debug;
    Corrade::Containers::Pointer<Corrade::Utility::Warning> _warning;
    Corrade::Containers::Pointer<Corrade::Utility::Error> _error;

    Magnum::DebugTools::GLFrameProfiler profiler;

    // scene graph

    Corrade::Containers::Array<Corrade::Containers::Optional<Magnum::GL::Mesh>> meshes;
    Corrade::Containers::Array<Corrade::Containers::Optional<Magnum::GL::Texture2D>> textures;
    Corrade::Containers::Array<Corrade::Containers::Optional<Magnum::Trade::PhongMaterialData>> materials;

    Scene3D scene;
    Object3D manipulator, cameraObject;
    Corrade::Containers::Pointer<Magnum::SceneGraph::Camera3D> camera;
    Magnum::SceneGraph::DrawableGroup3D drawables;
    Magnum::SceneGraph::DrawableGroup3D velocityDrawables; // moving objects that contribute to the velocity buffer

    Magnum::Shaders::Phong coloredMaterialShader;
    Magnum::Shaders::Phong texturedMaterialShader;

    // scene

    static constexpr float cameraNear = 0.5f;
    static constexpr float cameraFar = 50.0f;

    Magnum::Timeline timeline;
    Magnum::SceneGraph::AnimableGroup3D meshAnimables;
    Magnum::SceneGraph::AnimableGroup3D cameraAnimables;
    static constexpr size_t objectGridSize = 6;
    Corrade::Containers::Array<Magnum::Vector3> lightPositions;
    Corrade::Containers::Array<Magnum::Color4> lightColors;

    // checkerboard rendering

    Magnum::GL::Framebuffer velocityFramebuffer;
    Magnum::GL::Texture2D velocityAttachment;
    Magnum::GL::Texture2D velocityDepthAttachment;

    VelocityShader velocityShader;

    static constexpr size_t FRAMES = 2;
    static constexpr size_t JITTERED_FRAME = 1;
    size_t currentFrame;

    // checkerboard framebuffers
    // quarter size (half width, half height)
    Magnum::GL::Framebuffer framebuffers[FRAMES];
    Magnum::GL::MultisampleTexture2DArray colorAttachments;
    Magnum::GL::MultisampleTexture2DArray depthAttachments;

    DepthBlitShader depthBlitShader;

    Magnum::GL::Framebuffer outputFramebuffer;
    Magnum::GL::Texture2D outputColorAttachment;

    ReconstructionShader reconstructionShader;

    Options options;
};
