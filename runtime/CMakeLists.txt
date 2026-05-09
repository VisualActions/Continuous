add_executable(continuous_runtime WIN32 src/main.cpp)
target_link_libraries(continuous_runtime PRIVATE continuous::engine)
set_target_properties(continuous_runtime PROPERTIES
    OUTPUT_NAME "ContinuousRuntime"
    FOLDER "Apps"
)
