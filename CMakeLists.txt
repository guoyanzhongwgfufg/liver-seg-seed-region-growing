cmake_minimum_required(VERSION 3.15)
project(liver_segmentation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ITK REQUIRED)
include(${ITK_USE_FILE})

find_package(VTK REQUIRED COMPONENTS 
    CommonCore 
    CommonDataModel 
    IOImage 
    InteractionImage 
    RenderingCore 
    RenderingFreeType 
    RenderingOpenGL2 
    InteractionStyle 
)
include(${VTK_USE_FILE})

add_executable(liver_segmentation_v2 src/seed_seg.cpp)
# add_executable(seed_from_mask src/seed_from_mask.cpp)

target_link_libraries(liver_segmentation_v2 PRIVATE ${ITK_LIBRARIES} ${VTK_LIBRARIES})
# target_link_libraries(seed_from_mask PRIVATE ${ITK_LIBRARIES} ${VTK_LIBRARIES})




# # #---------------old--------------------------
# # cmake_minimum_required(VERSION 3.15)
# # project(liver_lung_segmentation)

# # set(CMAKE_CXX_STANDARD 17)
# # set(CMAKE_CXX_STANDARD_REQUIRED ON)
# # set(CMAKE_CXX_EXTENSIONS OFF)

# # if(MSVC)
# #   add_compile_options("/utf-8")
# # endif()

# # # 由 vcpkg 工具链处理路径，千万不要手动设置 ITK_DIR、ZLIB_DIR 等
# # find_package(double-conversion CONFIG REQUIRED)
# # find_package(ZLIB REQUIRED)
# # find_package(ITK REQUIRED)
# # include(${ITK_USE_FILE})
# # find_package(VTK REQUIRED COMPONENTS
# #   vtkRenderingCore
# #   vtkRenderingFreeType
# #   vtkInteractionStyle
# #   vtkInteractionImage
  
# # )

# # # 可执行程序
# # add_executable(liver_lung_segmentation src/main.cpp)
# # # add_executable(liver_lung_segmentation src/official_demo.cpp)
# # # add_executable(liver_lung_segmentation test/test.cpp)
# # # add_executable(med_sam test/med_sam.cpp)

# # # 链接所有依赖库
# # target_link_libraries(liver_lung_segmentation PRIVATE
# #   ${ITK_LIBRARIES}
# #   ${VTK_LIBRARIES}
# #   double-conversion::double-conversion
# #   ZLIB::ZLIB
# #   ${double-conversion_LIBRARIES}
# # )

# # # 包含头文件路径（VTK 推荐用 include_directories 也可以）
# # target_include_directories(liver_lung_segmentation PRIVATE ${VTK_INCLUDE_DIRS})



