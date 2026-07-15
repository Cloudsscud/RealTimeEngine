# CMakeList.txt: RealTimeEngine 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required(VERSION 3.8)

# 如果支持，请为 MSVC 编译器启用热重载。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT 
  "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("RealTimeEngine")

# 依赖配置
set(VULKAN_SDK "D:/A1_Project_C_Cpp/Dependencies/vulkan/VulkanSDK/1.4.350.0")
set(GLM_DIR "D:/A1_Project_C_Cpp/Dependencies/glm")
set(GLFW_SOURCE_DIR "D:/A1_Project_C_Cpp/Dependencies/GLFW/glfw-3.4/glfw-3.4")

# glfw子模块
add_subdirectory(${GLFW_SOURCE_DIR} ${CMAKE_BINARY_DIR}/glfw)

# 添加可执行文件
add_executable(RealTimeEngine "src/RealTimeEngine.cpp" "include/RealTimeEngine.h")

# 设置C++标准
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET RealTimeEngine PROPERTY CXX_STANDARD 17)
endif()

# 添加包含目录
target_include_directories(RealTimeEngine PRIVATE
    ${VULKAN_SDK}/Include
    ${GLM_DIR}
)

# 链接库
target_link_libraries(RealTimeEngine
    ${VULKAN_SDK}/Lib/vulkan-1.lib
    glfw
)

