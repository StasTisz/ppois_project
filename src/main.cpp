#include <iostream>
#include <string>
#include <cstdlib>
#include <memory>

#include "repository/DatabaseManager.h"
#include "repository/SQLiteMovieRepository.h"
#include "repository/SQLiteUserRepository.h"
#include "recommendation/RecommendationService.h"
#include "bot/BotState.h"

int main() {
    std::cout << "========================================\n";
    std::cout << " Movie Recommendation Telegram Bot (C++)\n";
    std::cout << " Backend & Recommendation Facade (Task 1.1 - 1.5)\n";
    std::cout << "========================================\n\n";

    const std::string dbPath = "data/movies.db";
    repository::DatabaseManager dbManager;

    std::cout << "[DB] Opening SQLite database: " << dbPath << " ...\n";
    if (!dbManager.open(dbPath)) {
        std::cerr << "[ERROR] Could not open database: " << dbPath << "\n";
        return 1;
    }

    std::cout << "[DB] Initializing database schema...\n";
    if (!dbManager.initializeSchema()) {
        std::cerr << "[ERROR] Failed to initialize schema!\n";
        return 1;
    }

    std::cout << "[DB] Seeding dataset from data/initial_movies.json ...\n";
    dbManager.seedMoviesFromJson("data/initial_movies.json");

    auto movieRepo = std::make_shared<repository::SQLiteMovieRepository>(dbManager.getHandle());
    auto userRepo = std::make_shared<repository::SQLiteUserRepository>(dbManager.getHandle());

    size_t movieCount = movieRepo->count();
    std::cout << "[OK] Total movies in database: " << movieCount << "\n\n";

    // Initialize Recommendation Service Facade
    recommendation::RecommendationService recService(movieRepo, userRepo);

    // 1. New user with only favorite genres (Auto -> Popularity / Cold-Start)
    int64_t newUser = 1001;
    userRepo->saveUser({newUser, "new_user", 0, 1700000000});
    recService.setFavoriteGenres(newUser, {"Sci-Fi", "Animation"});

    std::cout << "[SERVICE] User " << newUser << " (No ratings yet):\n";
    std::cout << "  Auto strategy resolved: "
              << recService.resolveStrategyName(newUser, recommendation::RecommendationMode::Auto) << "\n";
    auto recsNew = recService.getRecommendations(newUser, 3);
    for (size_t i = 0; i < recsNew.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << recsNew[i].title 
                  << " (" << recsNew[i].year << ") - "
                  << "Rating: " << recsNew[i].ratingAvg << "/10\n";
    }

    // 2. Experienced user with positive rating (Auto -> Content-Based)
    int64_t nolanFan = 1002;
    userRepo->saveUser({nolanFan, "nolan_fan", 0, 1700000000});
    recService.rateMovie(nolanFan, 10, 10); // Rated Interstellar (id 10) with 10/10

    std::cout << "\n[SERVICE] User " << nolanFan << " (Rated 'Interstellar' with 10/10):\n";
    std::cout << "  Auto strategy resolved: "
              << recService.resolveStrategyName(nolanFan, recommendation::RecommendationMode::Auto) << "\n";
    auto recsFan = recService.getRecommendations(nolanFan, 3);
    for (size_t i = 0; i < recsFan.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << recsFan[i].title 
                  << " (" << recsFan[i].year << ", Dir: " << recsFan[i].director << ") - "
                  << "Rating: " << recsFan[i].ratingAvg << "/10\n";
    }

    // 3. Similar movies query (e.g. for "More like this" bot button)
    std::cout << "\n[SERVICE] Movies similar to 'Inception' (id 5):\n";
    auto similar = recService.getSimilarMovies(5, 3);
    for (size_t i = 0; i < similar.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << similar[i].title 
                  << " (" << similar[i].year << ", Dir: " << similar[i].director << ")\n";
    }

    const char* tokenEnv = std::getenv("TELEGRAM_BOT_TOKEN");
    std::cout << "\n[BOT] Telegram Bot Status:\n";
    if (!tokenEnv || std::string(tokenEnv).empty()) {
        std::cout << "  - TELEGRAM_BOT_TOKEN is not set in environment.\n"
                  << "  - Waiting for Dev 2 (Stas) event loop integration.\n";
    } else {
        std::cout << "  - TELEGRAM_BOT_TOKEN found.\n";
    }

    std::cout << "\n[DONE] Backend & RecommendationService Facade ready!\n";
    return 0;
}
