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
find_package(TgBot CONFIG QUIET)
if(TARGET TgBot::TgBot)
    set(TGBOT_TARGET TgBot::TgBot)
else()
    find_package(tgbot-cpp CONFIG REQUIRED)
    set(TGBOT_TARGET tgbot-cpp::tgbot-cpp)
endif()

# Core domain interface library
add_library(movie_rec_core INTERFACE)

target_include_directories(movie_rec_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(movie_rec_core INTERFACE
    ${SQLITE3_TARGET}
    nlohmann_json::nlohmann_json
    ${TGBOT_TARGET}
)

# Bot implementation library
add_library(movie_rec_bot_lib STATIC
    src/bot/BotEventRouter.cpp
    src/bot/TelegramBotApp.cpp
    src/bot/BotStateMachine.cpp
    src/bot/KeyboardFactory.cpp
    src/bot/CommandDispatcher.cpp
    src/bot/states/DefaultState.cpp
    src/bot/states/GenreSelectionState.cpp
    src/bot/states/BrowsingRecommendationsState.cpp
    src/bot/states/RatingState.cpp
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
    tests/test_bot_infra.cpp
    tests/test_bot_fsm.cpp
)

target_link_libraries(movie_rec_tests PRIVATE
    movie_rec_bot_lib
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(movie_rec_tests)
