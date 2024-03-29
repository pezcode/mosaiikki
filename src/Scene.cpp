#include "Scene.h"

#include <Magnum/Trade/SceneData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/TextureData.h>
#include <Magnum/Trade/ImageData.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Constants.h>
#include <Magnum/Math/FunctionsBatch.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/MeshTools/Compile.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/PluginManager/Manager.h>

using namespace Magnum;
using namespace Magnum::Math::Literals;
using namespace Corrade;
using namespace Feature;

// for some reason GCC expects a definition for a static constexpr float
constexpr Magnum::Float Scene::shininess;

Scene::Scene(NoCreateT) : materialShader(NoCreate), velocityShader(NoCreate) { }

Scene::Scene() : materialShader(NoCreate), velocityShader(NoCreate)
{
    // Default material

    Containers::Array<GL::Texture2D> defaultTextures = defaultMaterial.createTextures({ 4, 4 });
    for(GL::Texture2D& texture : defaultTextures)
        Containers::arrayAppend(textures, Containers::pointer<GL::Texture2D>(std::move(texture)));

    // Scene

    // w in light positions decides about light type (1 = point, 0 = directional)
    lightPositions = Containers::array<Vector4>({ { -3.0f, 10.0f, 10.0f, 0.0f } });
    lightColors = Containers::array<Color3>({ 0xffffff_rgbf });
    CORRADE_INTERNAL_ASSERT(lightPositions.size() == lightColors.size());

    cameraObject.setParent(&scene);
    cameraObject.translate(Vector3::zAxis(-5.0f));
    camera.reset(new SceneGraph::Camera3D(cameraObject));

    RotationAnimable3D& camAnimable =
        cameraObject.addFeature<RotationAnimable3D>(Vector3::yAxis(), 15.0_degf, 45.0_degf);
    cameraAnimables.add(camAnimable);
    camAnimable.setState(SceneGraph::AnimationState::Running);

    root.setParent(&scene);

    // Shaders

    // vertex color is coming from the instance buffer attribute
    materialShader =
        Shaders::PhongGL(Shaders::PhongGL::Flag::InstancedTransformation | Shaders::PhongGL::Flag::VertexColor |
                             Shaders::PhongGL::Flag::DiffuseTexture | Shaders::PhongGL::Flag::SpecularTexture |
                             Shaders::PhongGL::Flag::NormalTexture,
                         lightPositions.size());
    materialShader.setLightPositions(lightPositions);
    materialShader.setLightColors(lightColors);
    materialShader.setLabel("Material shader (instanced, textured Phong)");

    velocityShader = VelocityShader(VelocityShader::Flag::InstancedTransformation);
    velocityShader.setLabel("Velocity shader (instanced)");

    // Objects

    const char* mesh = "resources/models/Avocado/Avocado.gltf";

    Object3D& object = root.addChild<Object3D>();
    object.translate({ 0.0f, 0.0f, -5.0f });

    Range3D bounds;
    bool loaded = loadScene(mesh, object, &bounds);
    CORRADE_ASSERT(loaded, "Failed to load scene", );

    float scale = 2.0f / bounds.size().max();

    object.scaleLocal(Vector3(scale));

    Vector3 center(float(objectGridSize - 1) / 2.0f);

    // animated objects + associated velocity drawables

    for(TexturedDrawable3D* drawable : featuresInChildren<TexturedDrawable3D>(object))
    {
        Object3D& drawableObject = static_cast<Object3D&>(drawable->object());

        Magnum::UnsignedInt id = drawable->meshId();

        VelocityDrawable3D& velocityDrawable = drawableObject.addFeature<VelocityDrawable3D>(
            velocityShader, id, *velocityMeshes[id], *velocityInstanceBuffers[id]);
        velocityDrawables.add(velocityDrawable);

        VelocityDrawable3D& transparentVelocityDrawable = drawableObject.addFeature<VelocityDrawable3D>(
            velocityShader, id, *velocityMeshes[id], *velocityInstanceBuffers[id]);
        transparentVelocityDrawables.add(transparentVelocityDrawable);

        for(size_t z = 0; z < objectGridSize; z++)
        {
            for(size_t y = 0; y < objectGridSize; y++)
            {
                for(size_t x = 0; x < objectGridSize; x++)
                {
                    Object3D& instance = drawableObject.addChild<Object3D>();

                    Matrix3 toLocal = Matrix3(instance.absoluteTransformationMatrix()).inverted();

                    // add instances back to front for alpha blending
                    Vector3 translation = (Vector3(x, y, -float(objectGridSize - z - 1)) - center) * 4.0f;
                    instance.translate(toLocal * translation);

                    InstanceDrawable3D& instanceDrawable = drawable->addInstance(instance);

                    bool transparent = z == (objectGridSize - 1);
                    Color3 color =
                        (Color3(x, y, z) + Color3(1.0f)) / objectGridSize; // +1 to avoid completely black objects
                    float alpha = transparent ? 0.75f : 1.0f;
                    instanceDrawable.setColor(Color4(color, alpha));

                    Vector3 localX = toLocal * Vector3::xAxis();
                    Vector3 localY = toLocal * Vector3::yAxis();

                    if(transparent)
                        transparentVelocityDrawable.addInstance(instance);
                    else
                        velocityDrawable.addInstance(instance);

                    TranslationAnimable3D& translationAnimable = instance.addFeature<TranslationAnimable3D>(
                        localX, 5.5f * localX.length(), 3.0f * localX.length());
                    meshAnimables.add(translationAnimable);
                    translationAnimable.setState(SceneGraph::AnimationState::Running);

                    RotationAnimable3D& rotationAnimable =
                        instance.addFeature<RotationAnimable3D>(localY, 90.0_degf, Rad(Constants::inf()));
                    meshAnimables.add(rotationAnimable);
                    rotationAnimable.setState(SceneGraph::AnimationState::Running);
                }
            }
        }
    }
}

bool Scene::loadScene(const char* file, Object3D& root, Range3D* bounds)
{
    // load importer

    PluginManager::Manager<Trade::AbstractImporter> manager;
    Containers::Pointer<Trade::AbstractImporter> importer = manager.loadAndInstantiate("AnySceneImporter");
    if(!importer)
        return false;

    // load scene

    if(!importer->openFile(file))
        return false;

    if(importer->sceneCount() == 0)
        return false;

    Int sceneId = importer->defaultScene();
    if(sceneId == -1)
        sceneId = 0;
    Containers::Optional<Trade::SceneData> sceneData = importer->scene(sceneId);
    if(!sceneData)
        return false;

    if(!sceneData->is3D() ||
       !sceneData->hasField(Trade::SceneField::Parent) || // TODO does this break files with just one object?
       !sceneData->hasField(Trade::SceneField::Mesh))
        return false;

    // load and compile meshes

    Magnum::UnsignedInt meshOffset = meshes.size();
    Containers::arrayResize(meshes, meshes.size() + importer->meshCount());
    Containers::arrayResize(instanceBuffers, instanceBuffers.size() + importer->meshCount());
    Containers::arrayResize(velocityMeshes, velocityMeshes.size() + importer->meshCount());
    Containers::arrayResize(velocityInstanceBuffers, velocityInstanceBuffers.size() + importer->meshCount());

    Range3D sceneBounds;

    for(UnsignedInt i = 0; i < importer->meshCount(); i++)
    {
        Containers::Optional<Trade::MeshData> data = importer->mesh(i);
        if(data && data->hasAttribute(Trade::MeshAttribute::Position) &&
           data->hasAttribute(Trade::MeshAttribute::Normal) && data->hasAttribute(Trade::MeshAttribute::Tangent) &&
           data->hasAttribute(Trade::MeshAttribute::TextureCoordinates) &&
           GL::meshPrimitive(data->primitive()) == GL::MeshPrimitive::Triangles)
        {
            if(bounds)
                sceneBounds = Math::join(sceneBounds, Range3D(Math::minmax(data->positions3DAsArray())));

            GL::Buffer indices, vertices;
            indices.setData(data->indexData());
            vertices.setData(data->vertexData());

            meshes[meshOffset + i].emplace(MeshTools::compile(*data, indices, vertices));
            instanceBuffers[meshOffset + i].emplace(InstanceDrawable3D::addInstancedBuffer(*meshes[i]));

            velocityMeshes[meshOffset + i].emplace(MeshTools::compile(*data, indices, vertices));
            velocityInstanceBuffers[meshOffset + i].emplace(
                VelocityInstanceDrawable3D::addInstancedBuffer(*velocityMeshes[i]));
        }
        else
            Warning(Warning::Flag::NoSpace)
                << "Skipping mesh " << i << " (must be a triangle mesh with normals, tangents and UV coordinates)";
    }

    if(bounds)
        *bounds = sceneBounds;

    // load materials

    Magnum::UnsignedInt materialOffset = materials.size();
    Containers::arrayResize(materials, materials.size() + importer->materialCount());

    for(UnsignedInt i = 0; i < importer->materialCount(); i++)
    {
        Containers::Optional<Trade::MaterialData> data = importer->material(i);
        if(data && data->types() & Trade::MaterialType::Phong)
        {
            materials[materialOffset + i].emplace(std::move(*data));
        }
        else
            Warning(Warning::Flag::NoSpace) << "Skipping material " << i << " (not Phong-compatible)";
    }

    // load textures

    Magnum::UnsignedInt textureOffset = textures.size();
    Containers::arrayResize(textures, textures.size() + importer->textureCount());

    for(UnsignedInt i = 0; i < importer->textureCount(); i++)
    {
        Containers::Optional<Trade::TextureData> textureData = importer->texture(i);
        if(textureData && textureData->type() == Trade::TextureType::Texture2D)
        {
            Containers::Optional<Trade::ImageData2D> imageData = importer->image2D(textureData->image(), 0 /* level */);
            if(imageData)
            {
                GL::TextureFormat format;
                switch(imageData->format())
                {
                    case PixelFormat::RGB8Unorm:
                        format = GL::TextureFormat::RGB8;
                        break;
                    case PixelFormat::RGBA8Unorm:
                        format = GL::TextureFormat::RGBA8;
                        break;
                    default:
                        Warning(Warning::Flag::NoSpace)
                            << "Skipping texture " << i << " (unsupported format " << imageData->format() << ")";
                        continue;
                }
                GL::Texture2D texture;
                texture.setMagnificationFilter(textureData->magnificationFilter())
                    .setMinificationFilter(textureData->minificationFilter(), textureData->mipmapFilter())
                    .setWrapping(textureData->wrapping().xy())
                    .setStorage(Math::log2(imageData->size().max()) + 1, format, imageData->size())
                    .setSubImage(0, {}, *imageData)
                    .generateMipmap();
                textures[textureOffset + i].emplace(std::move(texture));
            }
        }
    }

    // load objects

    Containers::Array<Object3D*> objects { size_t(sceneData->mappingBound()) };
    const auto parents = sceneData->parentsAsArray();
    // create objects in the scene graph
    for(const Containers::Pair<UnsignedInt, Int>& parent : parents)
    {
        objects[parent.first()] = &root.addChild<Object3D>();
    }

    // set parents, separate pass because children can occur
    // before parents
    for(const Containers::Pair<UnsignedInt, Int>& parent : parents)
    {
        Object3D* parentObject = parent.second() == -1 ? &root : objects[parent.second()];
        objects[parent.first()]->setParent(parentObject);
    }

    for(const Containers::Pair<UnsignedInt, Matrix4>& transformation : sceneData->transformations3DAsArray())
    {
        if(Object3D* object = objects[transformation.first()])
            object->setTransformation(transformation.second());
    }

    for(const Containers::Pair<UnsignedInt, Containers::Pair<UnsignedInt, Int>>& meshMaterial :
        sceneData->meshesMaterialsAsArray())
    {
        if(Object3D* object = objects[meshMaterial.first()])
        {
            bool useDefaultMaterial = true;
            const UnsignedInt meshId = meshOffset + meshMaterial.second().first();
            const Int materialId = meshMaterial.second().second();
            if(materialId != -1 && materials[materialOffset + materialId])
            {
                const Trade::PhongMaterialData& material =
                    materials[materialOffset + materialId]->as<Trade::PhongMaterialData>();
                if(TexturedDrawable3D::isCompatibleMaterial(material, materialShader))
                {
                    TexturedDrawable3D& drawable =
                        object->addFeature<TexturedDrawable3D>(materialShader,
                                                               meshId,
                                                               *meshes[meshId],
                                                               *instanceBuffers[meshId],
                                                               textures.exceptPrefix(textureOffset),
                                                               material,
                                                               shininess);
                    drawables.add(drawable);
                    useDefaultMaterial = false;
                }
            }

            if(useDefaultMaterial)
            {
                const Trade::PhongMaterialData& material = defaultMaterial.as<Trade::PhongMaterialData>();
                TexturedDrawable3D& drawable =
                    object->addFeature<TexturedDrawable3D>(materialShader,
                                                           meshId,
                                                           *meshes[meshId],
                                                           *instanceBuffers[meshId],
                                                           textures, // first textures are the default textures
                                                           material,
                                                           material.shininess());
                drawables.add(drawable);
            }

            // by default, there are no instances
            // add an InstanceDrawable3D to drawable.instanceDrawables() for each instance
        }
    }

    return true;
}
