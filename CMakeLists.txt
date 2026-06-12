cmake_minimum_required(VERSION 3.15)
project(CSS_Educational_ESP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Sources ---
# Manual listing is preferred for professional projects to ensure build reliability
set(PROJECT_SOURCES
    "src/main.cpp"
    "src/Core/Memory.cpp"
    "src/Game/Entity.cpp"
    "src/Overlay/Overlay.cpp"
    
    "imgui/imgui.cpp"
    "imgui/imgui_draw.cpp"
    "imgui/imgui_tables.cpp"
    "imgui/imgui_widgets.cpp"
    "imgui/imgui_impl_dx9.cpp"
    "imgui/imgui_impl_win32.cpp"
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# --- Includes ---
# Using target-based includes (Modern CMake)
target_include_directories(${PROJECT_NAME} PRIVATE 
    "include"
    "imgui"
)

# --- Libraries ---
target_link_libraries(${PROJECT_NAME} PRIVATE 
    d3d9 
    dwmapi
)

# --- Compiler Options ---
if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX-) # Warning level 4
endif()
