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

set(ENGINE_SOURCES
    src/main.cpp
    src/first_app.cpp
    src/czx_window.cpp
    src/czx_device.cpp
    src/czx_pipeline.cpp
)

set(ENGINE_HEADERS
    include/first_app.h
    include/czx_window.h
    include/czx_device.h
    include/czx_pipeline.h
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

# 设置C++标准
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET RealTimeEngine PROPERTY CXX_STANDARD 17)
endif()

# 添加包含目录
target_include_directories(RealTimeEngine PRIVATE
    include
    ${VULKAN_SDK}/Include
    ${GLM_DIR}
)

# 链接库
target_link_libraries(RealTimeEngine
    ${VULKAN_SDK}/Lib/vulkan-1.lib
    glfw
)


file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/shaders/)
set_target_properties(RealTimeEngine PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
