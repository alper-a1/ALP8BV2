# (for now) barebones 

add_executable(assembler
    main.cpp
    lexer.cpp
    metadata_pass.cpp
    macro_pass.cpp
    label_pass.cpp
    codegen.cpp
    write_binary.cpp
)

target_include_directories(assembler PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
# alp8b_file defined in root cmakelists
target_link_libraries(assembler PRIVATE alp8b_file)

# Copy __built-ins.asm into the build directory alongside the compiled binary
# REQURIED since we search for builtins based on the location of the executable
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/__built-ins.asm"
    "${CMAKE_CURRENT_BINARY_DIR}/__built-ins.asm"
    COPYONLY
)

target_compile_options(assembler PRIVATE -Wall -Wextra)