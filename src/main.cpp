#include <iostream>
#include <string>
#include <cstdlib>
#include <memory>

#include "repository/DatabaseManager.h"
#include "repository/SQLiteMovieRepository.h"
#include "repository/SQLiteUserRepository.h"
#include "recommendation/RecommendationService.h"
#include "bot/BotConfig.h"
#include "bot/TelegramBotApp.h"

/**
 * @file main.cpp
 * @brief Точка входа в приложение Telegram-бота рекомендаций фильмов.
 *
 * Инициализирует локальную базу данных SQLite, загружает датасет фильмов,
 * настраивает сервис рекомендаций и конечный автомат FSM,
 * и запускает цикл опроса Telegram Bot API (long-polling).
 */

/**
 * @brief Главная функция программы.
 * @return 0 при успешном запуске и выполнении либо штатном выходе при отсутствии токена; 1 при фатальной ошибке.
 */
int main() {
    std::cout << "========================================\n";
    std::cout << " Movie Recommendation Telegram Bot (C++)\n";
    std::cout << " Full Integration: Core + Bot Layer     \n";
    std::cout << "========================================\n\n";

    bot::BotConfig config = bot::BotConfig::fromEnv();

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
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Необработанное исключение в TelegramBotApp: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
