add_executable(continuous_cooker main.cpp)
target_link_libraries(continuous_cooker PRIVATE continuous::engine)
set_target_properties(continuous_cooker PROPERTIES
    OUTPUT_NAME "cooker"
    FOLDER "Tools"
)
