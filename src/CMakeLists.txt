set(SOURCES
    main.cpp
    Mosaiikki.h
    Mosaiikki.cpp
    ImGuiApplication.h
    ImGuiApplication.cpp
    Scene.h
    Scene.cpp
    Drawables/TexturedDrawable.h
    Drawables/InstanceDrawable.h
    Drawables/VelocityDrawable.h
    Drawables/VelocityInstanceDrawable.h
    Animables/AxisTranslationAnimable.h
    Animables/AxisRotationAnimable.h
    DefaultMaterial.h
    Feature.h
    Options.h
    Shaders/VelocityShader.h
    Shaders/VelocityShader.cpp
    Shaders/DepthBlitShader.h
    Shaders/DepthBlitShader.cpp
    Shaders/ReconstructionShader.h
    Shaders/ReconstructionShader.cpp
    Shaders/ReconstructionOptions.h
)

set(SHADERS
    Shaders/ReconstructionShader.vert
    Shaders/ReconstructionShader.frag
    Shaders/VelocityShader.vert
    Shaders/VelocityShader.frag
    Shaders/DepthBlitShader.vert
    Shaders/DepthBlitShader.frag
)

source_group("Shader Files" FILES ${SHADERS})

if(CORRADE_TARGET_MSVC)
    # this is required to turn off automatic scaling and
    # get the DPI scaling factor from Magnum/SDL
    list(APPEND SOURCES windows-dpi-awareness.manifest)
endif()

# enable compiler warnings
set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS ON)

find_package(Corrade REQUIRED
    Main
    Utility
)
find_package(Magnum REQUIRED
    GlfwApplication
    GL
    SceneGraph
    Shaders
    Trade
    MeshTools
    DebugTools
    AnySceneImporter
    AnyImageImporter
)
find_package(MagnumPlugins REQUIRED
    GltfImporter
    StbImageImporter
)
find_package(MagnumIntegration REQUIRED
    ImGui
)

# embed resources
corrade_add_resource(RESOURCES "${PROJECT_SOURCE_DIR}/resources/resources.conf")
corrade_add_resource(SHADER_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/resources.conf")

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${SHADERS} ${RESOURCES} ${SHADER_RESOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Corrade::Main
    Corrade::Utility
    Magnum::Magnum
    Magnum::Application # alias to application class being used
    Magnum::GL
    Magnum::SceneGraph
    Magnum::Shaders
    Magnum::Trade
    Magnum::MeshTools
    Magnum::AnySceneImporter
    Magnum::AnyImageImporter
    Magnum::DebugTools
    MagnumPlugins::GltfImporter
    MagnumPlugins::StbImageImporter
    MagnumIntegration::ImGui
)

# warnings
target_include_directories(${PROJECT_NAME} SYSTEM INTERFACE "${PROJECT_SOURCE_DIR}/3rdparty")
if(CORRADE_TARGET_MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /wd26812 # The enum type X is unscoped. Prefer 'enum class' over 'enum'
    )
endif()

# TODO GLSL -> GLSL conversion once implemented in Magnum
if(SHADER_VALIDATION)
    find_package(Magnum REQUIRED
        AnyShaderConverter
        shaderconverter
    )
    find_package(MagnumPlugins REQUIRED
        GlslangShaderConverter
    )
    # need to tell shaderconverter to link against static plugins
    # this requires CMake 3.13 since the target was defined in another directory
    target_link_libraries(magnum-shaderconverter PRIVATE MagnumPlugins::GlslangShaderConverter Magnum::AnyShaderConverter)

    foreach(SHADER ${SHADERS})
        add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
            # glslang has no validation support for GL_ARB_texture_multisample
            # so we need to use GLSL 3.2 (150) which supports sampler2DMSArray
            COMMAND Magnum::shaderconverter --validate --input-version "150" --output-version opengl -D VALIDATION "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}"
        )
    endforeach()
endif()

# link/copy resources folder to binary dir
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.14.0")
    file(CREATE_LINK "${PROJECT_SOURCE_DIR}/resources" "${PROJECT_BINARY_DIR}/resources" COPY_ON_ERROR SYMBOLIC)
else()
    file(COPY "${PROJECT_SOURCE_DIR}/resources" DESTINATION "${PROJECT_BINARY_DIR}")
endif()

# install binary + resources
install(TARGETS ${PROJECT_NAME} DESTINATION "${MAGNUM_DEPLOY_PREFIX}")
install(DIRECTORY
    "${PROJECT_SOURCE_DIR}/resources"
    DESTINATION "${MAGNUM_DEPLOY_PREFIX}"
)

# Make the executable a default target to build & run in Visual Studio
set_property(DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
# set debug working directory (default is CMAKE_CURRENT_BINARY_DIR)
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
