find_package(Qt6 REQUIRED COMPONENTS Test)

foreach(name zoommath keynames annotations settings capture breaktimer demoscript)
    add_executable(test_${name} test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE appcore Qt6::Test)
    add_test(NAME ${name} COMMAND test_${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

# Helper for the KDE end-to-end test.
add_executable(kwinshot tools/kwinshot.cpp)
target_link_libraries(kwinshot PRIVATE Qt6::Gui Qt6::DBus)
