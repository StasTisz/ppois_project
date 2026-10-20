#include <iostream>
#include <string>
#include <cstdlib>

#include "repository/DatabaseManager.h"
#include "repository/SQLiteMovieRepository.h"
#include "repository/SQLiteUserRepository.h"
#include "bot/BotState.h"

int main() {
    std::cout << "========================================\n";
    std::cout << " Movie Recommendation Telegram Bot (C++)\n";
    std::cout << " Backend & SQLite Repositories (Task 1.1 & 1.2)\n";
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

    repository::SQLiteMovieRepository movieRepo(dbManager.getHandle());
    repository::SQLiteUserRepository userRepo(dbManager.getHandle());

    size_t movieCount = movieRepo.count();
    std::cout << "[OK] Total movies in database: " << movieCount << "\n\n";

    std::cout << "[INFO] Top 3 movies in database:\n";
    auto topMovies = movieRepo.getTopRated(3);
    for (size_t i = 0; i < topMovies.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << topMovies[i].title 
                  << " (" << topMovies[i].year << ") - "
                  << "Rating: " << topMovies[i].ratingAvg << "/10\n";
    }

    const char* tokenEnv = std::getenv("TELEGRAM_BOT_TOKEN");
    std::cout << "\n[BOT] Telegram Bot Status:\n";
    if (!tokenEnv || std::string(tokenEnv).empty()) {
        std::cout << "  - TELEGRAM_BOT_TOKEN is not set in environment.\n"
                  << "  - Waiting for Dev 2 (Stas) event loop integration.\n";
    } else {
        std::cout << "  - TELEGRAM_BOT_TOKEN found.\n";
    }

    std::cout << "\n[DONE] Database & Repository layer ready!\n";
    return 0;
}
