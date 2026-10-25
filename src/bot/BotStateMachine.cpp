#include "bot/BotStateMachine.h"

#include "bot/CommandDispatcher.h"
#include "bot/states/BrowsingRecommendationsState.h"
#include "bot/states/DefaultState.h"
#include "bot/states/GenreSelectionState.h"
#include "bot/states/RatingState.h"
#include "recommendation/ContentBasedRecommendationStrategy.h"

#include <ctime>
#include <iostream>
#include <utility>

/**
 * @file BotStateMachine.cpp
 * @brief Реализация логики конечного автомата диалогов BotStateMachine.
 */

namespace bot {

BotStateMachine::BotStateMachine(std::shared_ptr<IBotMessenger> messenger)
    : messenger_(std::move(messenger)),
      commandDispatcher_(std::make_unique<CommandDispatcher>(*this, messenger_)) {
    registerDefaultStates();
    initFallbackMovies();
}

BotStateMachine::BotStateMachine(
    std::shared_ptr<IBotMessenger> messenger,
    std::shared_ptr<recommendation::RecommendationService> recService,
    std::shared_ptr<repository::IUserRepository> userRepo)
    : messenger_(std::move(messenger)),
      recService_(std::move(recService)),
      userRepo_(std::move(userRepo)),
      commandDispatcher_(std::make_unique<CommandDispatcher>(*this, messenger_)) {
    registerDefaultStates();
    initFallbackMovies();
}

BotStateMachine::~BotStateMachine() = default;

void BotStateMachine::registerDefaultStates() {
    registerState(std::make_shared<DefaultState>());
    registerState(std::make_shared<GenreSelectionState>());
    registerState(std::make_shared<BrowsingRecommendationsState>());
    registerState(std::make_shared<RatingState>());
}

void BotStateMachine::initFallbackMovies() {
    core::Movie m1;
    m1.id = 1;
    m1.title = "Начало (Inception)";
    m1.year = 2010;
    m1.genres = {"Sci-Fi", "Action"};
    m1.director = "Кристофер Нолан";
    m1.description = "Кобб — искусный вор, крадущий ценные секреты из глубин подсознания во время сна.";
    m1.ratingAvg = 8.8;
    m1.posterUrl = "https://image.tmdb.org/t/p/w500/edv5CZvWj09upOsy2Y6IwDhK8bt.jpg";

    core::Movie m2;
    m2.id = 2;
    m2.title = "Побег из Шоушенка";
    m2.year = 1994;
    m2.genres = {"Drama"};
    m2.director = "Фрэнк Дарабонт";
    m2.description = "Бухгалтер Энди Дюфрейн обвинён в убийстве и оказывается в тюрьме Шоушенк.";
    m2.ratingAvg = 9.3;
    m2.posterUrl = "https://image.tmdb.org/t/p/w500/q6y0Go1tsGEsmtFryDOJo3dEmqu.jpg";

    core::Movie m3;
    m3.id = 3;
    m3.title = "Темный рыцарь";
    m3.year = 2008;
    m3.genres = {"Action", "Crime", "Drama"};
    m3.director = "Кристофер Нолан";
    m3.description = "Бэтмен вступает в битву с криминальным гением Джокером за спасение Готэма.";
    m3.ratingAvg = 9.0;
    m3.posterUrl = "https://image.tmdb.org/t/p/w500/qJ2tW6WMUDux911r6m7haRef0WH.jpg";

    core::Movie m4;
    m4.id = 4;
    m4.title = "Интерстеллар";
    m4.year = 2014;
    m4.genres = {"Sci-Fi", "Drama"};
    m4.director = "Кристофер Нолан";
    m4.description = "Группа исследователей отправляется в путешествие сквозь червоточину в поисках нового дома для человечества.";
    m4.ratingAvg = 8.7;
    m4.posterUrl = "https://image.tmdb.org/t/p/w500/gEU2QniE6E77NI6lCU6MxlNBvIx.jpg";

    fallbackMovies_ = {m1, m2, m3, m4};
}

void BotStateMachine::setRecommendationService(
    std::shared_ptr<recommendation::RecommendationService> recService,
    std::shared_ptr<repository::IUserRepository> userRepo) {
    recService_ = std::move(recService);
    userRepo_ = std::move(userRepo);
}

void BotStateMachine::registerState(std::shared_ptr<IDialogState> state) {
    if (state) {
        states_[state->getStateType()] = std::move(state);
    }
}

UserSession& BotStateMachine::getSession(int64_t userId, int64_t chatId, const std::string& username) {
    auto it = sessions_.find(userId);
    if (it != sessions_.end()) {
        if (chatId != 0) {
            it->second.chatId = chatId;
        }
        if (!username.empty()) {
            it->second.username = username;
        }
        return it->second;
    }

    UserSession newSession;
    newSession.userId = userId;
    newSession.chatId = chatId;
    newSession.username = username;
    newSession.state = BotState::Idle;

    // Обеспечение согласованности с базой данных SQLite
    if (userRepo_) {
        auto existingUser = userRepo_->getById(userId);
        if (!existingUser.has_value()) {
            int64_t nowSec = static_cast<int64_t>(std::time(nullptr));
            userRepo_->saveUser(core::User{userId, username, static_cast<int>(BotState::Idle), nowSec});
        } else {
            newSession.state = static_cast<BotState>(existingUser->stateId);
        }
    }

    // Подгрузка сохраненных жанров и оценок из RecommendationService / SQLite
    if (recService_) {
        auto savedGenres = recService_->getFavoriteGenres(userId);
        if (!savedGenres.empty()) {
            newSession.selectedGenres = std::move(savedGenres);
        }
        auto ratings = recService_->getUserRatings(userId);
        for (const auto& r : ratings) {
            newSession.movieRatings[r.movieId] = r.score;
        }
    }

    auto [insertedIt, _] = sessions_.emplace(userId, std::move(newSession));
    return insertedIt->second;
}

void BotStateMachine::transitionTo(int64_t userId, BotState newState) {
    auto& session = getSession(userId);
    session.state = newState;

    if (userRepo_) {
        int64_t nowSec = static_cast<int64_t>(std::time(nullptr));
        userRepo_->saveUser(core::User{session.userId, session.username, static_cast<int>(newState), nowSec});
    }

    auto it = states_.find(newState);
    if (it != states_.end() && it->second) {
        it->second->onEnter(*this, session);
    }
}

std::shared_ptr<IBotMessenger> BotStateMachine::getMessenger() const {
    return messenger_;
}

void BotStateMachine::saveUserGenres(int64_t userId, const std::vector<std::string>& genres) {
    auto& session = getSession(userId);
    session.selectedGenres = genres;
    session.skippedMovieIds.clear();

    if (userRepo_) {
        auto existing = userRepo_->getById(userId);
        if (!existing.has_value()) {
            int64_t nowSec = static_cast<int64_t>(std::time(nullptr));
            userRepo_->saveUser(core::User{session.userId, session.username, static_cast<int>(session.state), nowSec});
        }
    }

    if (recService_) {
        recService_->setFavoriteGenres(userId, genres);
    }
}

void BotStateMachine::recordMovieRating(int64_t userId, int64_t movieId, int score) {
    auto& session = getSession(userId);
    session.movieRatings[movieId] = score;
    session.skippedMovieIds.erase(movieId);

    if (userRepo_) {
        auto existing = userRepo_->getById(userId);
        if (!existing.has_value()) {
            int64_t nowSec = static_cast<int64_t>(std::time(nullptr));
            userRepo_->saveUser(core::User{session.userId, session.username, static_cast<int>(session.state), nowSec});
        }
    }

    if (recService_) {
        recService_->rateMovie(userId, movieId, score);
    }
}

std::optional<core::Movie> BotStateMachine::fetchNextMovie(int64_t userId) {
    if (movieProvider_) {
        return movieProvider_(userId);
    }

    auto& session = getSession(userId);

    // Если подключен RecommendationService, запрашиваем рекомендации из ядра
    if (recService_) {
        // Синхронизируем жанры, если они выбраны в сессии
        auto savedInRec = recService_->getFavoriteGenres(userId);
        if (savedInRec.empty() && !session.selectedGenres.empty()) {
            recService_->setFavoriteGenres(userId, session.selectedGenres);
        }

        auto candidates = recService_->getRecommendations(userId, 20);

        // 1. Сначала ищем фильм, совпадающий с выбранными жанрами
        if (!session.selectedGenres.empty()) {
            for (const auto& movie : candidates) {
                if (!session.isSkipped(movie.id) && session.movieRatings.find(movie.id) == session.movieRatings.end()) {
                    for (const auto& g : movie.genres) {
                        if (session.hasGenre(g)) {
                            return movie;
                        }
                    }
                }
            }
        }

        // 2. Если точных совпадений по жанрам нет — берем любого первого неоцененного кандидата
        for (const auto& movie : candidates) {
            if (!session.isSkipped(movie.id) && session.movieRatings.find(movie.id) == session.movieRatings.end()) {
                return movie;
            }
        }

        // 3. Если все кандидаты были пропущены пользователем, сбрасываем фильтр пропусков
        if (!candidates.empty()) {
            session.skippedMovieIds.clear();
            if (!session.selectedGenres.empty()) {
                for (const auto& movie : candidates) {
                    if (session.movieRatings.find(movie.id) == session.movieRatings.end()) {
                        for (const auto& g : movie.genres) {
                            if (session.hasGenre(g)) {
                                return movie;
                            }
                        }
                    }
                }
            }
            for (const auto& movie : candidates) {
                if (session.movieRatings.find(movie.id) == session.movieRatings.end()) {
                    return movie;
                }
            }
        }

        return std::nullopt;
    }

    // Автономный режим с fallback-списком фильмов
    if (!session.selectedGenres.empty()) {
        for (const auto& movie : fallbackMovies_) {
            if (session.movieRatings.find(movie.id) != session.movieRatings.end() || session.isSkipped(movie.id)) {
                continue;
            }
            for (const auto& g : movie.genres) {
                if (session.hasGenre(g)) {
                    return movie;
                }
            }
        }
    }

    for (const auto& movie : fallbackMovies_) {
        if (session.movieRatings.find(movie.id) == session.movieRatings.end() && !session.isSkipped(movie.id)) {
            return movie;
        }
    }

    // Если все не оцененные фильмы в fallback были пропущены — очищаем пропуски и повторяем
    session.skippedMovieIds.clear();
    for (const auto& movie : fallbackMovies_) {
        if (session.movieRatings.find(movie.id) == session.movieRatings.end()) {
            return movie;
        }
    }

    return std::nullopt;
}

std::optional<core::Movie> BotStateMachine::getMovieById(int64_t movieId) const {
    if (recService_) {
        auto details = recService_->getMovieDetails(movieId);
        if (details.has_value()) {
            return details;
        }
    }

    for (const auto& movie : fallbackMovies_) {
        if (movie.id == movieId) {
            return movie;
        }
    }
    return std::nullopt;
}

std::vector<core::Movie> BotStateMachine::fetchSimilarMovies(int64_t movieId, size_t count) {
    if (recService_) {
        return recService_->getSimilarMovies(movieId, count);
    }
    return {};
}

std::string BotStateMachine::getActiveStrategyName(int64_t userId) const {
    if (recService_) {
        return recService_->resolveStrategyName(userId);
    }
    return "";
}

std::string BotStateMachine::buildRecommendationExplanation(int64_t userId, const core::Movie& movie) const {
    const auto& session = const_cast<BotStateMachine*>(this)->getSession(userId);

    // Собираем все положительные оценки пользователя (>= 7)
    std::vector<std::pair<int64_t, int>> highRatings;
    if (recService_) {
        auto userRatings = recService_->getUserRatings(userId);
        for (const auto& r : userRatings) {
            if (r.score >= 7) {
                highRatings.emplace_back(r.movieId, r.score);
            }
        }
    } else {
        for (const auto& [movieId, score] : session.movieRatings) {
            if (score >= 7) {
                highRatings.emplace_back(movieId, score);
            }
        }
    }

    if (highRatings.empty()) {
        return "💡 Почему рекомендуем: 🔥 Топ рейтинга в выбранных вами жанрах (Режим: Cold-Start / Popularity)";
    }

    // Ищем фильм с максимальным контентным сходством
    std::optional<core::Movie> bestSeedMovie;
    double maxSimilarity = -1.0;

    for (const auto& [seedId, score] : highRatings) {
        if (seedId == movie.id) continue;
        auto seedOpt = getMovieById(seedId);
        if (!seedOpt.has_value()) continue;

        double sim = recommendation::ContentBasedRecommendationStrategy::calculateMovieSimilarity(*seedOpt, movie);
        if (sim > maxSimilarity) {
            maxSimilarity = sim;
            bestSeedMovie = seedOpt;
        }
    }

    if (!bestSeedMovie.has_value()) {
        return "💡 Почему рекомендуем: 🔥 Топ рейтинга в выбранных вами жанрах (Режим: Cold-Start / Popularity)";
    }

    // Находим общие жанры
    std::vector<std::string> commonGenres;
    for (const auto& g : movie.genres) {
        if (std::find(bestSeedMovie->genres.begin(), bestSeedMovie->genres.end(), g) != bestSeedMovie->genres.end()) {
            commonGenres.push_back(g);
        }
    }

    bool sameDirector = (!movie.director.empty() && movie.director == bestSeedMovie->director);

    std::ostringstream oss;
    oss << "💡 Почему рекомендуем: 🧠 Умный подбор — похож на понравившийся вам «"
        << bestSeedMovie->title << "» (";

    bool hasMeta = false;
    if (sameDirector) {
        oss << "реж. " << movie.director;
        hasMeta = true;
    }

    if (!commonGenres.empty()) {
        if (hasMeta) oss << ", ";
        oss << "жанры: ";
        for (size_t i = 0; i < commonGenres.size(); ++i) {
            oss << commonGenres[i];
            if (i + 1 < commonGenres.size()) oss << ", ";
        }
        hasMeta = true;
    }

    if (!hasMeta) {
        oss << "высокое сходство";
    }

    oss << ")";
    return oss.str();
}

CommandDispatcher& BotStateMachine::getCommandDispatcher() {
    return *commandDispatcher_;
}

void BotStateMachine::setMovieProvider(MovieProvider provider) {
    movieProvider_ = std::move(provider);
}

void BotStateMachine::processMessage(const IncomingMessage& msg) {
    if (msg.text.empty()) {
        return;
    }

    // Сначала проверяем глобальные команды диспетчера (/start, /recommend, /genres, /history, /help)
    if (commandDispatcher_ && commandDispatcher_->dispatch(msg)) {
        return;
    }

    auto& session = getSession(msg.userId, msg.chatId, msg.username);
    auto it = states_.find(session.state);
    if (it != states_.end() && it->second) {
        it->second->handleMessage(*this, session, msg);
    } else {
        transitionTo(session.userId, BotState::Idle);
    }
}

void BotStateMachine::processCallback(const IncomingCallback& cb) {
    auto& session = getSession(cb.userId, cb.chatId);
    auto it = states_.find(session.state);
    if (it != states_.end() && it->second) {
        it->second->handleCallback(*this, session, cb);
    }
}

void BotStateMachine::attachToRouter(BotEventRouter& router) {
    router.setMessageHandler([this](const IncomingMessage& msg) {
        processMessage(msg);
    });
    router.setCallbackHandler([this](const IncomingCallback& cb) {
        processCallback(cb);
    });
}

} // namespace bot
