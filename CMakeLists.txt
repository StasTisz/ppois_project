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

# Core domain & backend library
add_library(movie_rec_core STATIC
    src/repository/DatabaseManager.cpp
    src/repository/SQLiteMovieRepository.cpp
    src/repository/SQLiteUserRepository.cpp
    src/recommendation/PopularityRecommendationStrategy.cpp
    src/recommendation/ContentBasedRecommendationStrategy.cpp
    src/recommendation/CollaborativeFilteringStrategy.cpp
    src/recommendation/HybridRecommendationStrategy.cpp
    src/recommendation/RecommendationService.cpp
    src/analytics/MetricsCollector.cpp
)

target_include_directories(movie_rec_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(movie_rec_core PUBLIC
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
    src/bot/NetworkErrorHandler.cpp
    src/bot/MovieCardFormatter.cpp
    src/bot/states/DefaultState.cpp
    src/bot/states/GenreSelectionState.cpp
    src/bot/states/BrowsingRecommendationsState.cpp
    src/bot/states/RatingState.cpp
)

target_include_directories(movie_rec_bot_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(movie_rec_bot_lib PUBLIC
    movie_rec_core
    ${TGBOT_TARGET}
)

# Main Telegram Bot Executable
add_executable(movie_rec_bot
    src/main.cpp
)

target_link_libraries(movie_rec_bot PRIVATE
    movie_rec_bot_lib
    movie_rec_core
)

# Unit Tests Executable
add_executable(movie_rec_tests
    tests/test_main.cpp
    tests/test_repository.cpp
    tests/test_recommendation.cpp
    tests/test_service.cpp
    tests/test_pipeline.cpp
    tests/test_bot_infra.cpp
    tests/test_bot_fsm.cpp
    tests/test_keyboard_factory.cpp
    tests/test_command_dispatcher.cpp
    tests/test_movie_card.cpp
    tests/test_performance_optimizations.cpp
    tests/test_collaborative_hybrid_metrics.cpp
    tests/test_coverage_expansion.cpp
)

target_link_libraries(movie_rec_tests PRIVATE
    movie_rec_bot_lib
    movie_rec_core
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(movie_rec_tests)
