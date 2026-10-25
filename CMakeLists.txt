cmake_minimum_required(VERSION 3.20)

# Automatic vcpkg toolchain integration if not passed via preset or command line
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
    elseif(EXISTS "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
    endif()
endif()

project(MovieRecommendationBot LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS specific prefix paths for Homebrew (Apple Silicon & Intel)
if(APPLE)
    if(EXISTS "/opt/homebrew")
        list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
        if(EXISTS "/opt/homebrew/opt/openssl@3")
            list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/openssl@3")
        endif()
    elseif(EXISTS "/usr/local")
        list(APPEND CMAKE_PREFIX_PATH "/usr/local")
        if(EXISTS "/usr/local/opt/openssl@3")
            list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/openssl@3")
        endif()
    endif()
endif()

# Enable testing
enable_testing()

# Find packages provided by vcpkg or system
find_package(Threads REQUIRED)
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

# tgbot-cpp detection with fallback
find_package(TgBot CONFIG QUIET)
if(TARGET TgBot::TgBot)
    set(TGBOT_TARGET TgBot::TgBot)
else()
    find_package(tgbot-cpp CONFIG QUIET)
    if(TARGET tgbot-cpp::tgbot-cpp)
        set(TGBOT_TARGET tgbot-cpp::tgbot-cpp)
    else()
        # Fallback for manual or brew-installed tgbot-cpp
        find_package(OpenSSL REQUIRED)
        find_package(CURL REQUIRED)
        find_package(Boost REQUIRED COMPONENTS system)
        find_path(TGBOT_INCLUDE_DIR NAMES tgbot/tgbot.h)
        find_library(TGBOT_LIBRARY NAMES TgBot tgbot)

        if(TGBOT_INCLUDE_DIR AND TGBOT_LIBRARY)
            add_library(TgBot::TgBot UNKNOWN IMPORTED)
            set_target_properties(TgBot::TgBot PROPERTIES
                IMPORTED_LOCATION "${TGBOT_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${TGBOT_INCLUDE_DIR}"
                INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto;CURL::libcurl;Boost::system"
            )
            set(TGBOT_TARGET TgBot::TgBot)
        else()
            find_package(tgbot-cpp REQUIRED)
            set(TGBOT_TARGET tgbot-cpp::tgbot-cpp)
        endif()
    endif()
endif()

# Copy data folder to build binary directory for runtime tests
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Core domain library
add_library(movie_rec_core STATIC
    src/repository/DatabaseManager.cpp
    src/repository/SQLiteMovieRepository.cpp
    src/repository/SQLiteUserRepository.cpp
    src/recommendation/PopularityRecommendationStrategy.cpp
    src/recommendation/ContentBasedRecommendationStrategy.cpp
    src/recommendation/RecommendationService.cpp
)

target_include_directories(movie_rec_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(movie_rec_core PUBLIC
    ${SQLITE3_TARGET}
    nlohmann_json::nlohmann_json
    ${TGBOT_TARGET}
    Threads::Threads
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
    tests/test_repository.cpp
    tests/test_recommendation.cpp
    tests/test_service.cpp
    tests/test_pipeline.cpp
    tests/test_bot_infra.cpp
    tests/test_bot_fsm.cpp
    tests/test_keyboard_factory.cpp
    tests/test_command_dispatcher.cpp
    tests/test_movie_card.cpp
    tests/test_bot_integration.cpp
)

target_link_libraries(movie_rec_tests PRIVATE
    movie_rec_bot_lib
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(movie_rec_tests)
