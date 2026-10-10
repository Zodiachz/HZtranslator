find_package(Qt6 REQUIRED COMPONENTS Test)

function(hypers_add_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE hypers_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    # Make the Qt DLLs resolvable when ctest runs the test from the build tree.
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
endfunction()

hypers_add_test(tst_languages)
hypers_add_test(tst_providers)
hypers_add_test(tst_textsegmenter)
hypers_add_test(tst_retrypolicy)
hypers_add_test(tst_keymapping)
hypers_add_test(tst_translationservice)
