set(SOURCES
	main.cpp
	Mosaiikki.h
	Mosaiikki.cpp
	ImGuiApplication.h
	ImGuiApplication.cpp
	Scene.h
	Scene.cpp
	Drawables/VelocityDrawable.h
	Drawables/VelocityInstanceDrawable.h
	Drawables/ColoredDrawable.h
	Drawables/TexturedDrawable.h
	Drawables/InstanceDrawable.h
	Animables/AxisTranslationAnimable.h
	Animables/AxisRotationAnimable.h
	Feature.h
	Options.h
	MagnumShadersSampleInterpolationOverride.h
	MagnumShadersSampleInterpolationOverride.cpp
	Shaders/ReconstructionShader.h
	Shaders/ReconstructionShader.cpp
	Shaders/VelocityShader.h
	Shaders/VelocityShader.cpp
	Shaders/DepthBlitShader.h
	Shaders/DepthBlitShader.cpp
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

if(MSVC)
	# this is required to turn off automatic scaling and
	# get the DPI scaling factor from Magnum/SDL
	list(APPEND SOURCES windows-dpi-awareness.manifest)
endif()

# enable compiler warnings
set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS ON)

find_package(Corrade REQUIRED Main Utility)
find_package(Magnum REQUIRED
	GlfwApplication
	GL
	SceneGraph
	Shaders
	Trade
	MeshTools
	AnySceneImporter
	AnyImageImporter
	DebugTools
)
find_package(MagnumPlugins REQUIRED TinyGltfImporter StbImageImporter)
find_package(MagnumIntegration REQUIRED ImGui)

# embed resources
corrade_add_resource(RESOURCES "${PROJECT_SOURCE_DIR}/resources/resources.conf")
corrade_add_resource(SHADER_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/resources.conf")

add_executable(${CMAKE_PROJECT_NAME} WIN32 ${SOURCES} ${SHADERS} ${RESOURCES} ${SHADER_RESOURCES})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
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
	MagnumPlugins::TinyGltfImporter
	MagnumPlugins::StbImageImporter
	MagnumIntegration::ImGui
)

# copy resources folder
file(COPY ${CMAKE_SOURCE_DIR}/resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# install binary + resources
install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION ${MAGNUM_DEPLOY_PREFIX})
install(DIRECTORY
	${CMAKE_CURRENT_BINARY_DIR}/resources
	DESTINATION ${MAGNUM_DEPLOY_PREFIX})

# Make the executable a default target to build & run in Visual Studio
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CMAKE_PROJECT_NAME})
