#include <iostream>
#include <string>
#include <cstdlib>
#include <memory>

#include "repository/DatabaseManager.h"
#include "repository/SQLiteMovieRepository.h"
#include "repository/SQLiteUserRepository.h"
#include "recommendation/PopularityRecommendationStrategy.h"
#include "recommendation/ContentBasedRecommendationStrategy.h"
#include "bot/BotState.h"

int main() {
    std::cout << "========================================\n";
    std::cout << " Movie Recommendation Telegram Bot (C++)\n";
    std::cout << " Backend, Repositories & Rec Engine (Task 1.1 - 1.3)\n";
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

    // Demo Recommendation Strategy
    recommendation::PopularityRecommendationStrategy popularityStrategy(movieRepo, userRepo);
    std::cout << "[REC] Active Strategy: " << popularityStrategy.getName() << "\n";

    int64_t demoUserId = 12345;
    userRepo->saveUser({demoUserId, "demo_user", 0, 1700000000});
    userRepo->setFavoriteGenres(demoUserId, {"Sci-Fi", "Animation"});

    std::cout << "[REC] Generating top 3 recommendations for demo user (Favorite genres: Sci-Fi, Animation):\n";
    auto recs = popularityStrategy.recommend(demoUserId, 3);
    for (size_t i = 0; i < recs.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << recs[i].title 
                  << " (" << recs[i].year << ") - "
                  << "Rating: " << recs[i].ratingAvg << "/10\n";
    }

    // Demo Task 1.4: Content-Based Strategy
    recommendation::ContentBasedRecommendationStrategy contentStrategy(movieRepo, userRepo);
    std::cout << "\n[REC] Active Strategy: " << contentStrategy.getName() << "\n";

    int64_t fanUserId = 67890;
    userRepo->saveUser({fanUserId, "scifi_director_fan", 0, 1700000000});
    // User gave Interstellar (id 10) high score (10/10)
    userRepo->addRating({0, fanUserId, 10, 10, 1700000100});

    std::cout << "[REC] Content-based recommendations for user who liked 'Interstellar' (10/10):\n";
    auto cbRecs = contentStrategy.recommend(fanUserId, 3);
    for (size_t i = 0; i < cbRecs.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << cbRecs[i].title 
                  << " (" << cbRecs[i].year << ", Dir: " << cbRecs[i].director << ") - "
                  << "Rating: " << cbRecs[i].ratingAvg << "/10\n";
    }

    const char* tokenEnv = std::getenv("TELEGRAM_BOT_TOKEN");
    std::cout << "\n[BOT] Telegram Bot Status:\n";
    if (!tokenEnv || std::string(tokenEnv).empty()) {
        std::cout << "  - TELEGRAM_BOT_TOKEN is not set in environment.\n"
                  << "  - Waiting for Dev 2 (Stas) event loop integration.\n";
    } else {
        std::cout << "  - TELEGRAM_BOT_TOKEN found.\n";
    }

    std::cout << "\n[DONE] Backend & Recommendation Engine (Strategy Pattern) ready!\n";
    return 0;
}
