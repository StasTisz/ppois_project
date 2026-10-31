#include "bot/BotStateMachine.h"
#include "bot/BotConstants.h"
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
    if (recService_) {
        metricsCollector_ = recService_->getMetricsCollector();
    }
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
    fallbackMovies_ = {
        core::Movie{1, "Начало (Inception)", 2010, {"Sci-Fi", "Action"}, "Кристофер Нолан",
            "Кобб — искусный вор, крадущий ценные секреты из глубин подсознания во время сна.",
            8.8, "https://image.tmdb.org/t/p/w500/edv5CZvWj09upOsy2Y6IwDhK8bt.jpg"},
        core::Movie{2, "Побег из Шоушенка", 1994, {"Drama"}, "Фрэнк Дарабонт",
            "Бухгалтер Энди Дюфрейн обвинён в убийстве и оказывается в тюрьме Шоушенк.",
            9.3, "https://image.tmdb.org/t/p/w500/q6y0Go1tsGEsmtFryDOJo3dEmqu.jpg"},
        core::Movie{3, "Темный рыцарь", 2008, {"Action", "Crime", "Drama"}, "Кристофер Нолан",
            "Бэтмен вступает в битву с криминальным гением Джокером за спасение Готэма.",
            9.0, "https://image.tmdb.org/t/p/w500/qJ2tW6WMUDux911r6m7haRef0WH.jpg"},
        core::Movie{4, "Интерстеллар", 2014, {"Sci-Fi", "Drama"}, "Кристофер Нолан",
            "Группа исследователей отправляется в путешествие в поисках нового дома.",
            8.7, "https://image.tmdb.org/t/p/w500/gEU2QniE6E77NI6lCU6MxlNBvIx.jpg"}
    };
}

void BotStateMachine::setRecommendationService(
    std::shared_ptr<recommendation::RecommendationService> recService,
    std::shared_ptr<repository::IUserRepository> userRepo) {
    recService_ = std::move(recService);
    userRepo_ = std::move(userRepo);
    if (recService_ && !metricsCollector_) {
        metricsCollector_ = recService_->getMetricsCollector();
    }
}

void BotStateMachine::registerState(std::shared_ptr<IDialogState> state) {
    if (state) {
        states_[state->getStateType()] = std::move(state);
    }
}

std::shared_ptr<std::recursive_mutex> BotStateMachine::getUserMutex(int64_t userId) const {
    std::lock_guard<std::mutex> lock(userMutexMapLock_);
    auto it = userMutexes_.find(userId);
    if (it != userMutexes_.end()) {
        return it->second;
    }
    auto m = std::make_shared<std::recursive_mutex>();
    userMutexes_[userId] = m;
    return m;
}

void BotStateMachine::syncUserWithRepository(UserSession& session) {
    if (!userRepo_) return;
    auto existingUser = userRepo_->getById(session.userId);
    if (!existingUser.has_value()) {
        int64_t nowSec = static_cast<int64_t>(std::time(nullptr));
        userRepo_->saveUser(core::User{session.userId, session.username, static_cast<int>(BotState::Idle), nowSec});
    } else {
        session.state = static_cast<BotState>(existingUser->stateId);
    }
}

void BotStateMachine::syncUserWithRecService(UserSession& session) {
    if (!recService_) return;
    auto savedGenres = recService_->getFavoriteGenres(session.userId);
    if (!savedGenres.empty()) {
        session.setSelectedGenres(std::move(savedGenres));
    }
    auto ratings = recService_->getUserRatings(session.userId);
    for (const auto& r : ratings) {
        session.recordRating(r.movieId, r.score);
    }
}

UserSession& BotStateMachine::getSession(int64_t userId, int64_t chatId, const std::string& username) {
    std::lock_guard<std::mutex> lock(sessionMapMutex_);
    auto it = sessions_.find(userId);
    if (it != sessions_.end()) {
        if (chatId != 0) it->second.chatId = chatId;
        if (!username.empty()) it->second.username = username;
        return it->second;
    }

    UserSession newSession{userId, chatId, username, BotState::Idle};
    syncUserWithRepository(newSession);
    syncUserWithRecService(newSession);
    auto [insertedIt, _] = sessions_.emplace(userId, std::move(newSession));
    return insertedIt->second;
}

void BotStateMachine::transitionTo(int64_t userId, BotState newState) {
    auto userMutex = getUserMutex(userId);
    std::lock_guard<std::recursive_mutex> lock(*userMutex);
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
    auto userMutex = getUserMutex(userId);
    std::lock_guard<std::recursive_mutex> lock(*userMutex);
    auto& session = getSession(userId);
    session.setSelectedGenres(genres);
    session.clearSkipped();

    if (userRepo_ && !userRepo_->getById(userId).has_value()) {
        int64_t nowSec = static_cast<int64_t>(std::time(nullptr));
        userRepo_->saveUser(core::User{session.userId, session.username, static_cast<int>(session.state), nowSec});
    }

    if (recService_) {
        recService_->setFavoriteGenres(userId, genres);
    }
}

void BotStateMachine::recordMovieRating(int64_t userId, int64_t movieId, int score) {
    auto userMutex = getUserMutex(userId);
    std::lock_guard<std::recursive_mutex> lock(*userMutex);
    auto& session = getSession(userId);
    session.recordRating(movieId, score);

    if (userRepo_ && !userRepo_->getById(userId).has_value()) {
        int64_t nowSec = static_cast<int64_t>(std::time(nullptr));
        userRepo_->saveUser(core::User{session.userId, session.username, static_cast<int>(session.state), nowSec});
    }

    if (recService_) {
        recService_->rateMovie(userId, movieId, score);
    }
}

bool BotStateMachine::matchesSelectedGenres(const core::Movie& movie, const UserSession& session) {
    if (!session.hasSelectedGenres()) return false;
    for (const auto& g : movie.genres) {
        if (session.hasGenre(g)) return true;
    }
    return false;
}

std::optional<core::Movie> BotStateMachine::findCandidate(
    const std::vector<core::Movie>& list, const UserSession& session,
    bool checkGenres, bool checkSkipped) const {
    for (const auto& movie : list) {
        if (session.hasRating(movie.id)) continue;
        if (checkSkipped && session.isSkipped(movie.id)) continue;
        if (checkGenres && !matchesSelectedGenres(movie, session)) continue;
        return movie;
    }
    return std::nullopt;
}

std::optional<core::Movie> BotStateMachine::fetchNextFromService(UserSession& session) {
    if (recService_->getFavoriteGenres(session.userId).empty() && session.hasSelectedGenres()) {
        recService_->setFavoriteGenres(session.userId, session.getSelectedGenres());
    }
    auto candidates = recService_->getRecommendations(session.userId, kRecommendationBatchSize);
    if (session.hasSelectedGenres()) {
        if (auto m = findCandidate(candidates, session, true, true)) return m;
    }
    if (auto m = findCandidate(candidates, session, false, true)) return m;

    if (!candidates.empty()) {
        session.clearSkipped();
        if (session.hasSelectedGenres()) {
            if (auto m = findCandidate(candidates, session, true, false)) return m;
        }
        return findCandidate(candidates, session, false, false);
    }
    return std::nullopt;
}

std::optional<core::Movie> BotStateMachine::fetchNextFromFallback(UserSession& session) {
    if (session.hasSelectedGenres()) {
        if (auto m = findCandidate(fallbackMovies_, session, true, true)) return m;
    }
    if (auto m = findCandidate(fallbackMovies_, session, false, true)) return m;

    session.clearSkipped();
    return findCandidate(fallbackMovies_, session, false, false);
}

std::optional<core::Movie> BotStateMachine::fetchNextMovie(int64_t userId) {
    auto userMutex = getUserMutex(userId);
    std::lock_guard<std::recursive_mutex> lock(*userMutex);
    {
        std::lock_guard<std::mutex> globalLock(globalConfigMutex_);
        if (movieProvider_) return movieProvider_(userId);
    }
    auto& session = getSession(userId);
    if (recService_) {
        return fetchNextFromService(session);
    }
    return fetchNextFromFallback(session);
}

std::optional<core::Movie> BotStateMachine::getMovieById(int64_t movieId) const {
    if (recService_) {
        auto details = recService_->getMovieDetails(movieId);
        if (details.has_value()) return details;
    }
    for (const auto& movie : fallbackMovies_) {
        if (movie.id == movieId) return movie;
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

std::string BotStateMachine::getMetricsReport() const {
    std::shared_ptr<analytics::MetricsCollector> collector;
    {
        std::lock_guard<std::mutex> lock(globalConfigMutex_);
        collector = metricsCollector_;
    }
    if (collector) {
        return collector->formatReport();
    }
    return "📊 Метрики пока не собраны (коллектор не инициализирован).";
}

std::vector<std::pair<int64_t, int>> BotStateMachine::collectHighRatings(
    int64_t userId, const UserSession& session) const {
    std::vector<std::pair<int64_t, int>> highRatings;
    if (recService_) {
        for (const auto& r : recService_->getUserRatings(userId)) {
            if (r.score >= kPositiveRatingThreshold) highRatings.emplace_back(r.movieId, r.score);
        }
    } else {
        for (const auto& [movieId, score] : session.getMovieRatings()) {
            if (score >= kPositiveRatingThreshold) highRatings.emplace_back(movieId, score);
        }
    }
    return highRatings;
}

std::optional<core::Movie> BotStateMachine::findBestSeedMovie(
    const std::vector<std::pair<int64_t, int>>& highRatings, const core::Movie& targetMovie) const {
    std::optional<core::Movie> bestSeed;
    double maxSimilarity = -1.0;
    for (const auto& [seedId, score] : highRatings) {
        if (seedId == targetMovie.id) continue;
        auto seedOpt = getMovieById(seedId);
        if (!seedOpt.has_value()) continue;
        double sim = recommendation::ContentBasedRecommendationStrategy::calculateMovieSimilarity(
            *seedOpt, targetMovie);
        if (sim > maxSimilarity) {
            maxSimilarity = sim;
            bestSeed = seedOpt;
        }
    }
    return bestSeed;
}

static std::string formatExplanationMeta(const core::Movie& seedMovie, const core::Movie& targetMovie) {
    bool sameDirector = (!targetMovie.director.empty() && targetMovie.director == seedMovie.director);
    std::string meta;
    if (sameDirector) meta += "реж. " + targetMovie.director;
    std::string genreStr;
    for (const auto& g : targetMovie.genres) {
        if (std::find(seedMovie.genres.begin(), seedMovie.genres.end(), g) != seedMovie.genres.end()) {
            genreStr += (genreStr.empty() ? "жанры: " : ", ") + g;
        }
    }
    if (!genreStr.empty()) meta += (meta.empty() ? "" : ", ") + genreStr;
    return meta.empty() ? "высокое сходство" : meta;
}

std::string BotStateMachine::formatExplanation(const core::Movie& seedMovie, const core::Movie& targetMovie) {
    return "💡 Почему рекомендуем: 🧠 Умный подбор — похож на «" + seedMovie.title + "» (" +
           formatExplanationMeta(seedMovie, targetMovie) + ")";
}

std::string BotStateMachine::buildRecommendationExplanation(int64_t userId, const core::Movie& movie) const {
    const auto& session = const_cast<BotStateMachine*>(this)->getSession(userId);
    auto highRatings = collectHighRatings(userId, session);
    if (highRatings.empty()) {
        return "💡 Почему рекомендуем: 🔥 Топ рейтинга в выбранных вами жанрах (Режим: Cold-Start / Popularity)";
    }
    auto bestSeed = findBestSeedMovie(highRatings, movie);
    if (!bestSeed.has_value()) {
        return "💡 Почему рекомендуем: 🔥 Топ рейтинга в выбранных вами жанрах (Режим: Cold-Start / Popularity)";
    }
    return formatExplanation(*bestSeed, movie);
}

CommandDispatcher& BotStateMachine::getCommandDispatcher() {
    return *commandDispatcher_;
}

void BotStateMachine::setMovieProvider(MovieProvider provider) {
    std::lock_guard<std::mutex> lock(globalConfigMutex_);
    movieProvider_ = std::move(provider);
}

void BotStateMachine::processMessage(const IncomingMessage& msg) {
    if (msg.text.empty()) return;
    if (storageChatId_ != 0 && msg.chatId == storageChatId_) return;

    auto userMutex = getUserMutex(msg.userId);
    std::lock_guard<std::recursive_mutex> lock(*userMutex);

    if (commandDispatcher_ && commandDispatcher_->dispatch(msg)) return;

    auto& session = getSession(msg.userId, msg.chatId, msg.username);
    auto it = states_.find(session.state);
    if (it != states_.end() && it->second) {
        it->second->handleMessage(*this, session, msg);
    } else {
        transitionTo(session.userId, BotState::Idle);
    }
}

void BotStateMachine::processCallback(const IncomingCallback& cb) {
    if (storageChatId_ != 0 && cb.chatId == storageChatId_) return;

    auto userMutex = getUserMutex(cb.userId);
    std::lock_guard<std::recursive_mutex> lock(*userMutex);

    auto& session = getSession(cb.userId, cb.chatId);
    auto it = states_.find(session.state);
    if (it != states_.end() && it->second) {
        it->second->handleCallback(*this, session, cb);
    }
}

void BotStateMachine::attachToRouter(BotEventRouter& router) {
    router.setMessageHandler([this](const IncomingMessage& msg) { processMessage(msg); });
    router.setCallbackHandler([this](const IncomingCallback& cb) { processCallback(cb); });
}

void BotStateMachine::cachePosterFileId(int64_t movieId, const std::string& fileId) {
    PosterCacheCallback cb;
    {
        std::lock_guard<std::mutex> lock(globalConfigMutex_);
        cb = posterCacheCallback_;
    }
    if (cb) cb(movieId, fileId);
}

} // namespace bot
