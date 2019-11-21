set(SOURCES
	main.cpp
	Application.h
	Application.cpp
	ImGuiApplication.h
	ImGuiApplication.cpp
	Feature.h
	ColoredDrawable.h
	SingleAxisTranslationAnimable.h
	Shaders/ReconstructionShader.h
	Shaders/ReconstructionShader.cpp
	Shaders/ReconstructionShader.vert
	Shaders/ReconstructionShader.frag
)

if(MSVC)
	# this is required to turn off automatic scaling and
	# get the DPI scaling factor from Magnum/SDL
	list(APPEND SOURCES windows-dpi-awareness.manifest)
endif()

# Enable compiler warnings
set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS ON)

find_package(Corrade REQUIRED Utility)
find_package(Magnum REQUIRED
	Sdl2Application
	GL
	SceneGraph
	Shaders
	Trade
	MeshTools
	AnySceneImporter
)
find_package(MagnumPlugins REQUIRED TinyGltfImporter)
find_package(MagnumIntegration REQUIRED ImGui)

# embed shaders
corrade_add_resource(SHADER_RESOURCES Shaders/resources.conf)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${SHADER_RESOURCES})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
	Corrade::Utility
	Magnum::Magnum
	Magnum::Application # alias to application class being used
	Magnum::GL
	Magnum::SceneGraph
	Magnum::Shaders
	Magnum::Trade
	Magnum::MeshTools
	Magnum::AnySceneImporter
	MagnumPlugins::TinyGltfImporter
	MagnumIntegration::ImGui
)

if(MSVC)
	# hide console window
	# target_link_options requires CMake 3.13+
	# (is there an easy alternative for earlier versions?)
	#target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
endif()

# copy/embed resources folder

if(EMSCRIPTEN)
	# regenerated after every change. the copy_boilerplate target
	# is run every build to make sure it always gets copied from
	# the intermediate binary dir to the final output dir.
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index.html.in ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.html @ONLY)

	# copy boilerplate into build dir (HTML, CSS, scripts)
	add_custom_target(copy_boilerplate ALL
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${MAGNUM_EMSCRIPTENAPPLICATION_JS}
			${MAGNUM_WEBAPPLICATION_CSS}
			${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.html
			$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>)

	# pack resources into .data file
	# this is run every time the binary is regenerated
	# TODO? only do this at configuration time (execute_process)
	get_filename_component(EMSDK_DIR ${CMAKE_RANLIB} DIRECTORY)
	set(FILE_PACKAGER_SCRIPT "${EMSDK_DIR}/tools/file_packager.py")
	if(NOT EXISTS "${FILE_PACKAGER_SCRIPT}")
		# these paths work with 1.39.1, might break at some point
		message(FATAL_ERROR "path to file_packager.py not found")
	endif()
	find_package(Python REQUIRED)
	add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
		COMMAND ${Python_EXECUTABLE} "${FILE_PACKAGER_SCRIPT}"
			resources.data
			--preload "${CMAKE_SOURCE_DIR}/resources@resources"
			--exclude "*LICENSE*"
			--js-output=resources.js
		WORKING_DIRECTORY "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>")
	target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "SHELL:-s FORCE_FILESYSTEM=1") # SHELL: avoids deduplication
else()
	file(COPY ${CMAKE_SOURCE_DIR}/resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

# install binary + resources

install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION ${MAGNUM_DEPLOY_PREFIX})
# for Emscripten copy/install the boilerplate next to the executable
if(EMSCRIPTEN)
	install(FILES
		${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.html
		${MAGNUM_EMSCRIPTENAPPLICATION_JS}
		${MAGNUM_WEBAPPLICATION_CSS}
		DESTINATION ${MAGNUM_DEPLOY_PREFIX})
	install(FILES
		${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.js.mem
		${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.wasm
		${CMAKE_CURRENT_BINARY_DIR}/resources.js
		${CMAKE_CURRENT_BINARY_DIR}/resources.data
		DESTINATION ${MAGNUM_DEPLOY_PREFIX} OPTIONAL)
else()
	install(DIRECTORY
		${CMAKE_CURRENT_BINARY_DIR}/resources
		DESTINATION ${MAGNUM_DEPLOY_PREFIX})
endif()

# Make the executable a default target to build & run in Visual Studio
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CMAKE_PROJECT_NAME})
