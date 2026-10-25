#include <iostream>
#include <string>
#include <memory>
#include <cstdlib>
#include <vector>

#include "repository/DatabaseManager.h"
#include "repository/SQLiteMovieRepository.h"
#include "repository/SQLiteUserRepository.h"
#include "recommendation/RecommendationService.h"
#include "bot/BotConfig.h"
#include "bot/TelegramBotApp.h"

int main() {
    std::cout << "========================================\n";
    std::cout << " Movie Recommendation Telegram Bot (C++)\n";
    std::cout << " Full Stack: Core, DB & Telegram Bot App \n";
    std::cout << "========================================\n\n";

    // 1. Initialize SQLite Database
    const std::string dbPath = "data/movies.db";
    auto dbManager = std::make_shared<repository::DatabaseManager>();

    std::cout << "[DB] Opening SQLite database: " << dbPath << " ...\n";
    if (!dbManager->open(dbPath)) {
        std::cerr << "[ERROR] Could not open database: " << dbPath << "\n";
        return 1;
    }

    if (!dbManager->initializeSchema()) {
        std::cerr << "[ERROR] Failed to initialize database schema!\n";
        return 1;
    }

    dbManager->seedMoviesFromJson("data/initial_movies.json");

    auto movieRepo = std::make_shared<repository::SQLiteMovieRepository>(dbManager->getHandle());
    auto userRepo = std::make_shared<repository::SQLiteUserRepository>(dbManager->getHandle());
    auto recService = std::make_shared<recommendation::RecommendationService>(movieRepo, userRepo);

    std::cout << "[OK] Database ready. Movies in catalog: " << movieRepo->count() << "\n\n";

    // 2. Load Telegram configuration
    bot::BotConfig config = bot::BotConfig::fromEnv();
    if (!config.isValid()) {
        std::cout << "[INFO] Переменная окружения TELEGRAM_BOT_TOKEN (или BOT_TOKEN) не задана.\n"
                  << "       Для сетевого запуска бота укажите токен:\n"
                  << "       $env:TELEGRAM_BOT_TOKEN=\"<ВАШ_ТОКЕН_БОТА>\"\n"
                  << "       Штатное завершение программы (код возврата 0).\n";
        return 0;
    }

    std::cout << "[INFO] Инициализация TelegramBotApp с конфигурацией из окружения...\n";
    try {
        bot::TelegramBotApp app(config);
        auto sm = app.getStateMachine();
        if (sm) {
            sm->setMetricsCollector(recService->getMetricsCollector());
            sm->setPosterCacheCallback([movieRepo](int64_t movieId, const std::string& fileId) {
                movieRepo->updatePosterFileId(movieId, fileId);
                std::cout << "[PosterCache] Сохранен Telegram file_id для фильма ID=" << movieId << "\n";
            });
            sm->setMovieProvider([recService, userRepo, sm](int64_t userId) -> std::optional<core::Movie> {
                auto& session = sm->getSession(userId);
                auto user = userRepo->getById(userId);
                if (!user) {
                    core::User newUser;
                    newUser.telegramId = userId;
                    newUser.username = session.username.empty() ? ("user_" + std::to_string(userId)) : session.username;
                    newUser.stateId = 0;
                    newUser.createdAt = 0;
                    userRepo->saveUser(newUser);

                    if (!session.selectedGenres.empty()) {
                        std::vector<std::string> genres(session.selectedGenres.begin(), session.selectedGenres.end());
                        userRepo->setFavoriteGenres(userId, genres);
                    }
                    user = userRepo->getById(userId);
                }
                if (user) {
                    auto recs = recService->recommend(userId, 20);
                    for (const auto& m : recs) {
                        if (session.movieRatings.find(m.id) == session.movieRatings.end()) {
                            return m;
                        }
                    }
                }
                return std::nullopt;
            });
        }
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Необработанное исключение в TelegramBotApp: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
