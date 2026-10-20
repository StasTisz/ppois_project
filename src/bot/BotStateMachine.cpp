#include "bot/BotStateMachine.h"

#include "bot/CommandDispatcher.h"
#include "bot/states/BrowsingRecommendationsState.h"
#include "bot/states/DefaultState.h"
#include "bot/states/GenreSelectionState.h"
#include "bot/states/RatingState.h"

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

    core::Movie m2;
    m2.id = 2;
    m2.title = "Побег из Шоушенка";
    m2.year = 1994;
    m2.genres = {"Drama"};
    m2.director = "Фрэнк Дарабонт";
    m2.description = "Бухгалтер Энди Дюфрейн обвинён в убийстве и оказывается в тюрьме Шоушенк.";
    m2.ratingAvg = 9.3;

    core::Movie m3;
    m3.id = 3;
    m3.title = "Темный рыцарь";
    m3.year = 2008;
    m3.genres = {"Action", "Crime", "Drama"};
    m3.director = "Кристофер Нолан";
    m3.description = "Бэтмен вступает в битву с криминальным гением Джокером за спасение Готэма.";
    m3.ratingAvg = 9.0;

    core::Movie m4;
    m4.id = 4;
    m4.title = "Интерстеллар";
    m4.year = 2014;
    m4.genres = {"Sci-Fi", "Drama"};
    m4.director = "Кристофер Нолан";
    m4.description = "Группа исследователей отправляется в путешествие сквозь червоточину в поисках нового дома для человечества.";
    m4.ratingAvg = 8.7;

    fallbackMovies_ = {m1, m2, m3, m4};
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

    auto [insertedIt, _] = sessions_.emplace(userId, std::move(newSession));
    return insertedIt->second;
}

void BotStateMachine::transitionTo(int64_t userId, BotState newState) {
    auto& session = getSession(userId);
    session.state = newState;

    auto it = states_.find(newState);
    if (it != states_.end() && it->second) {
        it->second->onEnter(*this, session);
    }
}

std::shared_ptr<IBotMessenger> BotStateMachine::getMessenger() const {
    return messenger_;
}

std::optional<core::Movie> BotStateMachine::fetchNextMovie(int64_t userId) {
    if (movieProvider_) {
        return movieProvider_(userId);
    }

    auto& session = getSession(userId);

    // Сначала ищем неоцененный фильм с совпадением по любимым жанрам
    if (!session.selectedGenres.empty()) {
        for (const auto& movie : fallbackMovies_) {
            if (session.movieRatings.find(movie.id) != session.movieRatings.end()) {
                continue;
            }
            for (const auto& g : movie.genres) {
                if (session.hasGenre(g)) {
                    return movie;
                }
            }
        }
    }

    // Если точных совпадений по жанрам нет — берем любой первый неоцененный фильм
    for (const auto& movie : fallbackMovies_) {
        if (session.movieRatings.find(movie.id) == session.movieRatings.end()) {
            return movie;
        }
    }

    return std::nullopt;
}

std::optional<core::Movie> BotStateMachine::getMovieById(int64_t movieId) const {
    for (const auto& movie : fallbackMovies_) {
        if (movie.id == movieId) {
            return movie;
        }
    }
    return std::nullopt;
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
