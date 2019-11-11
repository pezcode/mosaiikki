set(SOURCES
    main.cpp
	Application.h
	Application.cpp
	ImGuiApplication.h
	ImGuiApplication.cpp
)

if(MSVC)
	# this is required to turn off automatic scaling and
	# get the DPI scaling factor from Magnum/SDL
	list(APPEND SOURCES windows-dpi-awareness.manifest)
endif()

# Enable compiler warnings
set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS ON)

# TODO? use EmscriptenApplication
# https://github.com/mosra/magnum-bootstrap/tree/base-emscripten
# Tried it with v2019.10 and got some odd linker errors about SDL2 even
# when building with WITH_SDL2APPLICATION=OFF
# For now this works fine with Emscripten's SDL2 emulation

find_package(Corrade REQUIRED Utility)
find_package(Magnum REQUIRED GL Sdl2Application)
find_package(MagnumPlugins REQUIRED)
find_package(MagnumIntegration REQUIRED ImGui)

corrade_add_resource(RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../resources/resources.conf")

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${RESOURCES})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
	Corrade::Utility
    Magnum::Magnum
    Magnum::Application # alias to application class being used
    Magnum::GL
    MagnumIntegration::ImGui
)

if(MSVC)
    # hide console window
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
endif()

if(EMSCRIPTEN)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index.html.in ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.html @ONLY)
endif()

# install binary (.exe/.js)
install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION ${MAGNUM_DEPLOY_PREFIX})
# for Emscripten copy/install the boilerplate next to the executable
if(EMSCRIPTEN)
    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MAGNUM_EMSCRIPTENAPPLICATION_JS}
            ${MAGNUM_WEBAPPLICATION_CSS}
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.html
            $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.html
        ${MAGNUM_EMSCRIPTENAPPLICATION_JS}
        ${MAGNUM_WEBAPPLICATION_CSS}
        DESTINATION ${MAGNUM_DEPLOY_PREFIX})
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.js.mem
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.wasm
        DESTINATION ${MAGNUM_DEPLOY_PREFIX} OPTIONAL)
else()

endif()

# Make the executable a default target to build & run in Visual Studio
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CMAKE_PROJECT_NAME})
