set(SOURCES
    main.cpp
)

set_directory_properties(PROPERTIES CORRADE_USE_PEDANTIC_FLAGS ON)

# TODO use EmscriptenApplication
# https://github.com/mosra/magnum-bootstrap/tree/base-emscripten

find_package(Magnum REQUIRED Sdl2Application GL)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    Magnum::Magnum
    Magnum::Application
    Magnum::GL
)

if(MSVC)
    # hide console window
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
endif()

if(EMSCRIPTEN)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index.html ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.html @ONLY)
endif()

# install binary
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