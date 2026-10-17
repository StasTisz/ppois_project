#include <iostream>
#include <string>
#include <cstdlib>

#include "bot/TelegramBotApp.h"

/**
 * @file main.cpp
 * @brief Точка входа в приложение Telegram-бота рекомендаций фильмов.
 *
 * Считывает переменные окружения (`BOT_TOKEN` или `TELEGRAM_BOT_TOKEN`),
 * инициализирует приложение Telegram-бота и запускает цикл опроса long-polling.
 * При отсутствии токена выводит справочную информацию и корректно завершается.
 */

/**
 * @brief Главная функция программы.
 * @return 0 при успешном запуске и выполнении либо штатном выходе при отсутствии токена; 1 при фатальной ошибке.
 */
int main() {
    std::cout << "========================================\n";
    std::cout << " Movie Recommendation Telegram Bot (C++)\n";
    std::cout << " Bot & Event Infrastructure (Task 2.1) \n";
    std::cout << "========================================\n\n";

    const char* tokenEnv = std::getenv("BOT_TOKEN");
    if (!tokenEnv || std::string(tokenEnv).empty()) {
        tokenEnv = std::getenv("TELEGRAM_BOT_TOKEN");
    }

    if (!tokenEnv || std::string(tokenEnv).empty()) {
        std::cout << "[INFO] Переменная окружения BOT_TOKEN (или TELEGRAM_BOT_TOKEN) не задана.\n"
                  << "       Для сетевого запуска бота укажите токен:\n"
                  << "       export BOT_TOKEN=\"<ВАШ_ТОКЕН_БОТА>\"\n"
                  << "       Штатное завершение программы (код возврата 0).\n";
        return 0;
    }

    std::cout << "[INFO] Инициализация TelegramBotApp с указанным токеном...\n";
    try {
        bot::TelegramBotApp app(tokenEnv);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Необработанное исключение в TelegramBotApp: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
