file(GLOB_RECURSE EngerTests_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE EngerTests_HEADERS CONFIGURE_DEPENDS src/*.h)

add_executable(EngerTests ${EngerTests_SOURCES} ${EngerTests_HEADERS})

target_include_directories(EngerTests PRIVATE src)

target_link_enger(EngerTests)

target_link_libraries(EngerTests PRIVATE doctest::doctest)
include(doctest)
doctest_discover_tests(EngerTests)

set_project_warnings(EngerTests)
set_project_settings(EngerTests)
enable_sanitizers(EngerTests)
