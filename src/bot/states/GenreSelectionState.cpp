#include "bot/states/GenreSelectionState.h"
#include "bot/KeyboardFactory.h"
#include "core/GenreMask.h"

#include <sstream>

/**
 * @file GenreSelectionState.cpp
 * @brief Реализация логики выбора жанров в состоянии BotState::SelectingGenres.
 */

namespace bot {

namespace {

std::string formatGenresLine(const std::vector<std::string>& genres) {
    if (genres.empty()) return "(пока ничего не выбрано)\n";
    std::string res;
    for (size_t i = 0; i < genres.size(); ++i) {
        res += genres[i] + (i + 1 < genres.size() ? ", " : "\n");
    }
    return res;
}

} // namespace

BotState GenreSelectionState::getStateType() const {
    return BotState::SelectingGenres;
}

void GenreSelectionState::onEnter(IDialogStateContext& ctx, UserSession& session) {
    sendStatusMessage(ctx, session,
        "🎬 Добро пожаловать в Movie Recommendation Bot!\n"
        "Ваш персональный ИИ-киносоветчик с каталогом из 60 культовых фильмов.\n\n"
        "Как работает подбор рекомендаций:\n"
        "1️⃣ Старт по жанрам (Cold-Start) — выберите любимые жанры, и бот предложит признанные шедевры.\n"
        "2️⃣ Умный подбор (Content-Based AI) — после лайка (👍) или высокой оценки (>=7) включается\n"
        "   анализ сходства жанров (70%), режиссера (25%) и эпохи (5%) для точных рекомендаций.\n");
}

void GenreSelectionState::handleDone(IDialogStateContext& ctx, UserSession& session) {
    if (!session.hasSelectedGenres()) {
        if (auto messenger = ctx.getMessenger()) {
            messenger->sendMessage(session.chatId,
                "⚠️ Пожалуйста, выберите хотя бы один любимый жанр перед продолжением.");
        }
        return;
    }
    ctx.saveUserGenres(session.userId, session.getSelectedGenres());
    ctx.transitionTo(session.userId, BotState::BrowsingRecommendations);
}

void GenreSelectionState::handleUnknownGenre(
    IDialogStateContext& ctx, UserSession& session, const std::string& text) {
    if (auto messenger = ctx.getMessenger()) {
        messenger->sendMessage(session.chatId,
            "⚠️ Не удалось распознать жанр: \"" + text + "\".\n"
            "Пожалуйста, используйте кнопки меню ниже для выбора жанров.");
    }
    sendStatusMessage(ctx, session);
}

void GenreSelectionState::handleTextGenre(
    IDialogStateContext& ctx, UserSession& session, const std::string& text) {
    if (core::stringToGenreBit(text) == core::GenreBit::Unknown) {
        handleUnknownGenre(ctx, session, text);
        return;
    }

    bool added = session.toggleGenre(text);
    ctx.saveUserGenres(session.userId, session.getSelectedGenres());
    std::string notice = added
        ? ("✅ Жанр '" + text + "' добавлен в предпочтения.\n")
        : ("❌ Жанр '" + text + "' удален из предпочтений.\n");
    sendStatusMessage(ctx, session, notice);
}

void GenreSelectionState::handleMessage(
    IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) {
    if (msg.text == "/done" || msg.text == "Готово" || msg.text == "готово") {
        handleDone(ctx, session);
    } else if (msg.text == "/cancel" || msg.text == "/menu") {
        ctx.transitionTo(session.userId, BotState::Idle);
    } else if (!msg.text.empty() && msg.text[0] != '/') {
        handleTextGenre(ctx, session, msg.text);
    } else {
        sendStatusMessage(ctx, session);
    }
}

void GenreSelectionState::handleToggleCallback(
    IDialogStateContext& ctx, UserSession& session, const std::string& data, int32_t messageId) {
    const std::string togglePrefix = "genre:toggle:";
    std::string genre = data.substr(togglePrefix.size());
    session.toggleGenre(genre);

    if (auto messenger = ctx.getMessenger()) {
        std::string text = buildStatusText(session,
            "🎬 Настройка предпочтений:\n"
            "Выберите ваши любимые жанры, чтобы алгоритм подбирал подходящие фильмы.\n");
        InlineKeyboard kb = KeyboardFactory::createGenreSelectionKeyboard(session.getSelectedGenres());
        messenger->editMessageWithKeyboard(session.chatId, messageId, text, kb);
    }
}

void GenreSelectionState::handleCallback(
    IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) {
    const std::string togglePrefix = "genre:toggle:";
    if (cb.data.rfind(togglePrefix, 0) == 0) {
        handleToggleCallback(ctx, session, cb.data, cb.messageId);
    } else if (cb.data == "genre:confirm" || cb.data == "genre:done") {
        handleDone(ctx, session);
    } else if (cb.data == "genre:cancel") {
        ctx.transitionTo(session.userId, BotState::Idle);
    }
}

std::string GenreSelectionState::buildStatusText(const UserSession& session, const std::string& prefix) {
    std::ostringstream oss;
    if (!prefix.empty()) oss << prefix << "\n";
    oss << "Текущие выбранные жанры: " << formatGenresLine(session.getSelectedGenres());
    oss << "Нажмите на жанр для добавления/удаления или «✅ Подтвердить выбор»:";
    return oss.str();
}

void GenreSelectionState::sendStatusMessage(
    IDialogStateContext& ctx, const UserSession& session, const std::string& prefix) {
    auto messenger = ctx.getMessenger();
    if (!messenger) {
        return;
    }
    std::string text = buildStatusText(session, prefix);
    messenger->sendMessageWithKeyboard(
        session.chatId, text,
        KeyboardFactory::createGenreSelectionKeyboard(session.getSelectedGenres()));
}

} // namespace bot
