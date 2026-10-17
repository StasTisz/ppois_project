cmake_minimum_required(VERSION 3.20)
project(MovieRecommendationBot LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
enable_testing()

# Find packages provided by vcpkg
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

# SQLite3 can be provided as unofficial-sqlite3 or standard SQLite3
find_package(unofficial-sqlite3 CONFIG QUIET)
if(TARGET unofficial::sqlite3::sqlite3)
    set(SQLITE3_TARGET unofficial::sqlite3::sqlite3)
else()
    find_package(SQLite3 REQUIRED)
    set(SQLITE3_TARGET SQLite::SQLite3)
endif()

# tgbot-cpp
find_package(tgbot-cpp CONFIG REQUIRED)

# Core domain interface library
add_library(movie_rec_core INTERFACE)

target_include_directories(movie_rec_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(movie_rec_core INTERFACE
    ${SQLITE3_TARGET}
    nlohmann_json::nlohmann_json
    tgbot-cpp::tgbot-cpp
)

# Bot implementation library
add_library(movie_rec_bot_lib STATIC
    src/bot/BotEventRouter.cpp
)

target_link_libraries(movie_rec_bot_lib PUBLIC
    movie_rec_core
)

# Main Telegram Bot Executable
add_executable(movie_rec_bot
    src/main.cpp
)

target_link_libraries(movie_rec_bot PRIVATE
    movie_rec_bot_lib
)

# Unit Tests Executable
add_executable(movie_rec_tests
    tests/test_main.cpp
)

target_link_libraries(movie_rec_tests PRIVATE
    movie_rec_bot_lib
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(movie_rec_tests)
