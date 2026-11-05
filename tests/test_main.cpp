#include <gtest/gtest.h>
#include "core/Movie.h"
#include "core/User.h"
#include "core/Rating.h"
#include "bot/BotState.h"
#include "bot/BotConfig.h"
#include "repository/IMovieRepository.h"
#include "repository/IUserRepository.h"
#include "recommendation/IRecommendationStrategy.h"

// Basic sanity test that core models instantiate correctly
TEST(CoreModelTest, MovieInstantiation) {
    core::Movie movie;
    movie.id = 1;
    movie.title = "Inception";
    movie.year = 2010;
    movie.genres = {"Sci-Fi", "Action"};
    movie.director = "Christopher Nolan";
    movie.ratingAvg = 8.8;

    EXPECT_EQ(movie.id, 1);
    EXPECT_EQ(movie.title, "Inception");
    EXPECT_EQ(movie.genres.size(), 2);
}

TEST(CoreModelTest, UserInstantiation) {
    core::User user;
    user.telegramId = 123456789;
    user.username = "test_user";
    user.stateId = static_cast<int>(bot::BotState::Idle);

    EXPECT_EQ(user.telegramId, 123456789);
    EXPECT_EQ(user.username, "test_user");
    EXPECT_EQ(bot::botStateToString(bot::BotState::Idle), "Idle");
}

TEST(CoreModelTest, RatingInstantiation) {
    core::Rating rating;
    rating.id = 1;
    rating.userId = 123456789;
    rating.movieId = 1;
    rating.score = 9;

    EXPECT_EQ(rating.score, 9);
}

TEST(BotConfigTest, DefaultConfigInitialization) {
    bot::BotConfig config;
    EXPECT_FALSE(config.isValid());
    EXPECT_EQ(config.longPollTimeoutSeconds, 10);
    EXPECT_TRUE(config.debugLogging);

    config.token = "123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11";
    EXPECT_TRUE(config.isValid());
}
