cmake_minimum_required(VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT 
  "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("RealTimeEngine"
        VERSION 1.0.0
        DESCRIPTION "Real-time rendering engine using Vulkan"
        LANGUAGES CXX
)

set(dependencies_dir "D:/A1_Project_C_Cpp/Dependencies" CACHE PATH "Path to dependencies")

# 依赖配置
set(VULKAN_SDK "${dependencies_dir}/vulkan/VulkanSDK/1.4.350.0" CACHE PATH "Vulkan SDK path")
set(GLM_DIR "${dependencies_dir}/glm" CACHE PATH "GLM directory")
set(GLFW_SOURCE_DIR "${dependencies_dir}/GLFW/glfw-3.4/glfw-3.4" CACHE PATH "GLFW source directory")
set(RAPID_OBJ_DIR "${dependencies_dir}/rapidobj" CACHE PATH "rapidobj source directory")
set(STB_IMAGE_DIR "${dependencies_dir}/stb-master" CACHE PATH "stb_image source directory")

# 查找 glslc 编译器（使用你compile.bat中的路径）
find_program(GLSL_COMPILER
    NAMES glslc glslc.exe
    PATHS "${VULKAN_SDK}/Bin"
    DOC "Vulkan shader compiler (glslc)"
    REQUIRED
)
message(STATUS "Found glslc: ${GLSL_COMPILER}")

# ========== 着色器自动编译 ==========
set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

# 创建输出目录
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# 收集所有着色器源文件（.vert 和 .frag）
file(GLOB SHADER_SOURCES
    "${SHADER_SRC_DIR}/*.vert"
    "${SHADER_SRC_DIR}/*.frag"
    # 如果需要其他类型，取消注释：
    # "${SHADER_SRC_DIR}/*.comp"
    # "${SHADER_SRC_DIR}/*.geom"
)

# 为每个着色器生成编译规则
set(SHADER_OUTPUTS "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(OUTPUT_FILE "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    list(APPEND SHADER_OUTPUTS ${OUTPUT_FILE})
    
    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
        COMMAND ${GLSL_COMPILER} ${SHADER} -o ${OUTPUT_FILE}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
        VERBATIM
    )
endforeach()

# 创建目标来编译所有着色器
add_custom_target(CompileShaders ALL
    DEPENDS ${SHADER_OUTPUTS}
    COMMENT "Compiling all shaders (glslc)"
)


set(ENGINE_SOURCES
    src/main.cpp
    src/first_app.cpp
    src/czx_window.cpp
    src/czx_device.cpp
    src/czx_pipeline.cpp
    src/czx_swap_chain.cpp
    src/czx_texture.cpp
    src/czx_model.cpp
    src/czx_game_object.cpp
    src/czx_renderer.cpp
    src/czx_camera.cpp
    src/czx_buffer.cpp
    src/czx_descriptor.cpp
    src/simple_render_system.cpp
    src/keyboard_movement_controller.cpp
    
)

set(ENGINE_HEADERS
    include/first_app.h
    include/czx_window.h
    include/czx_device.h
    include/czx_pipeline.h
    include/czx_swap_chain.h
    include/czx_texture.h
    include/czx_model.h
    include/czx_game_object.h
    include/czx_renderer.h
    include/czx_camera.h
    include/czx_buffer.h
    include/czx_descriptor.h
    include/czx_utils.h
    include/czx_frame_info.h
    include/simple_render_system.h
    include/keyboard_movement_controller.h
)

set(ENGINE_RESOURCES
    # 如果有资源文件，在此添加

)

# glfw子模块
add_subdirectory(${GLFW_SOURCE_DIR} ${CMAKE_BINARY_DIR}/glfw)

# 添加可执行文件
add_executable(${PROJECT_NAME}
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
    ${ENGINE_RESOURCES}
)

  # 着色器编译目标作为可执行文件的依赖
add_dependencies(${PROJECT_NAME} CompileShaders)

# 设置C++标准
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET RealTimeEngine PROPERTY CXX_STANDARD 20)
endif()

# 添加包含目录
target_include_directories(RealTimeEngine PRIVATE
    include
    ${VULKAN_SDK}/Include
    ${GLM_DIR}
    ${RAPID_OBJ_DIR}/include
    ${STB_IMAGE_DIR}/stb-master
)

# 链接库
target_link_libraries(RealTimeEngine
    ${VULKAN_SDK}/Lib/vulkan-1.lib
    glfw
)


# 复制编译后的着色器到可执行文件目录（运行时使用）
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SHADER_OUTPUT_DIR}
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
    COMMENT "Copying compiled shaders to executable directory"
)

# 保留原有复制（用于源码目录，可能不需要，但保留兼容）
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/shaders/)

set_target_properties(RealTimeEngine PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
