#include <iostream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <filesystem>
#include <memory>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#endif

#include "repository/DatabaseManager.h"
#include "repository/SQLiteMovieRepository.h"
#include "repository/SQLiteUserRepository.h"
#include "recommendation/RecommendationService.h"
#include "bot/BotConfig.h"
#include "bot/TelegramBotApp.h"

/**
 * @file main.cpp
 * @brief Точка входа в приложение Telegram-бота рекомендаций фильмов.
 */

namespace {

bot::TelegramBotApp* g_runningApp = nullptr;

void handleSignal(int signal) {
    if (signal == SIGINT) {
        std::cout << "\n[INFO] Получен сигнал прерывания (SIGINT / Ctrl+C). Корректная остановка бота...\n";
        if (g_runningApp) {
            g_runningApp->stop();
        }
    }
}

} // namespace

int main() {
#ifdef _WIN32
    // Настройка кодировки UTF-8 в консоли Windows для корректного отображения кириллицы
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::signal(SIGINT, handleSignal);

    std::cout << "========================================\n";
    std::cout << " Movie Recommendation Telegram Bot (C++)\n";
    std::cout << " Full Stack: Core, DB & Telegram Bot App \n";
    std::cout << "========================================\n\n";

    // 1. Load Telegram configuration and resolve paths
    bot::BotConfig config = bot::BotConfig::fromEnv();

    // Создаем родительскую директорию для базы данных при её наличии и отсутствии на диске
    if (std::filesystem::path(config.dbPath).has_parent_path() &&
        !std::filesystem::path(config.dbPath).parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(config.dbPath).parent_path(), ec);
    }

    std::cout << "[DB] Инициализация базы данных SQLite: " << config.dbPath << " ...\n";
    repository::DatabaseManager dbManager;
    if (!dbManager.open(config.dbPath)) {
        std::cerr << "[ERROR] Не удалось открыть базу данных SQLite: " << config.dbPath << "\n";
        return 1;
    }

    std::cout << "[DB] Инициализация схемы таблиц...\n";
    if (!dbManager.initializeSchema()) {
        std::cerr << "[ERROR] Не удалось инициализировать схему базы данных!\n";
        return 1;
    }

    std::cout << "[DB] Загрузка начального датасета фильмов из " << config.seedJsonPath << " ...\n";
    dbManager.seedMoviesFromJson(config.seedJsonPath);

    auto movieRepo = std::make_shared<repository::SQLiteMovieRepository>(dbManager.getHandle());
    auto userRepo = std::make_shared<repository::SQLiteUserRepository>(dbManager.getHandle());
    auto recService = std::make_shared<recommendation::RecommendationService>(movieRepo, userRepo);

    size_t movieCount = movieRepo->count();
    std::cout << "[OK] База данных готова! Всего фильмов в каталоге: " << movieCount << "\n\n";

    if (!config.isValid()) {
        std::cout << "[INFO] Переменная окружения BOT_TOKEN (или TELEGRAM_BOT_TOKEN) не задана.\n"
                  << "       База данных SQLite и RecommendationService успешно инициализированы и готовы.\n"
                  << "       Для сетевого запуска Telegram-бота укажите токен в .env или окружении:\n"
                  << "       export BOT_TOKEN=\"<ВАШ_ТОКЕН_БОТА>\"\n"
                  << "       Штатное завершение программы (код возврата 0).\n";
        return 0;
    }

    std::cout << "[INFO] Инициализация TelegramBotApp с RecommendationService и базой данных SQLite...\n";
    try {
        bot::TelegramBotApp app(config, recService, userRepo);
        auto sm = app.getStateMachine();
        if (sm) {
            sm->setMetricsCollector(recService->getMetricsCollector());
            sm->setPosterCacheCallback([movieRepo](int64_t movieId, const std::string& fileId) {
                movieRepo->updatePosterFileId(movieId, fileId);
                std::cout << "[PosterCache] Сохранен Telegram file_id для фильма ID=" << movieId << "\n";
            });
        }

        g_runningApp = &app;
        app.run();
        g_runningApp = nullptr;
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Необработанное исключение в TelegramBotApp: " << e.what() << "\n";
        g_runningApp = nullptr;
        return 1;
    }

    return 0;
}
