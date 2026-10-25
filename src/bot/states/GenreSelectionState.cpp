#include "bot/states/GenreSelectionState.h"
#include "bot/KeyboardFactory.h"

#include <sstream>

/**
 * @file GenreSelectionState.cpp
 * @brief Реализация логики выбора жанров в состоянии BotState::SelectingGenres.
 */

namespace bot {

BotState GenreSelectionState::getStateType() const {
    return BotState::SelectingGenres;
}

void GenreSelectionState::onEnter(IDialogStateContext& ctx, UserSession& session) {
    sendStatusMessage(ctx, session,
        "🎬 Настройка предпочтений:\n"
        "Выберите ваши любимые жанры, чтобы алгоритм подбирал подходящие фильмы.\n");
}

void GenreSelectionState::handleMessage(IDialogStateContext& ctx, UserSession& session, const IncomingMessage& msg) {
    auto messenger = ctx.getMessenger();

    if (msg.text == "/done" || msg.text == "Готово" || msg.text == "готово") {
        if (session.selectedGenres.empty()) {
            if (messenger) {
                messenger->sendMessage(session.chatId,
                    "⚠️ Пожалуйста, выберите хотя бы один любимый жанр перед продолжением.");
            }
            return;
        }
        ctx.saveUserGenres(session.userId, session.selectedGenres);
        ctx.transitionTo(session.userId, BotState::BrowsingRecommendations);
        return;
    }

    if (msg.text == "/cancel" || msg.text == "/menu") {
        ctx.transitionTo(session.userId, BotState::Idle);
        return;
    }

    // Если передан текст названия жанра
    if (!msg.text.empty() && msg.text[0] != '/') {
        bool added = session.toggleGenre(msg.text);
        ctx.saveUserGenres(session.userId, session.selectedGenres);
        std::string notice = added
            ? ("✅ Жанр '" + msg.text + "' добавлен в предпочтения.\n")
            : ("❌ Жанр '" + msg.text + "' удален из предпочтений.\n");
        sendStatusMessage(ctx, session, notice);
        return;
    }

    sendStatusMessage(ctx, session);
}

void GenreSelectionState::handleCallback(IDialogStateContext& ctx, UserSession& session, const IncomingCallback& cb) {
    const std::string togglePrefix = "genre:toggle:";
    if (cb.data.rfind(togglePrefix, 0) == 0) {
        std::string genre = cb.data.substr(togglePrefix.size());
        session.toggleGenre(genre);

        // НЕ вызываем ctx.saveUserGenres() на каждый клик - обновляем только в памяти
        if (auto messenger = ctx.getMessenger()) {
            std::string text = buildStatusText(session,
                "🎬 Настройка предпочтений:\n"
                "Выберите ваши любимые жанры, чтобы алгоритм подбирал подходящие фильмы.\n");
            InlineKeyboard kb = KeyboardFactory::createGenreSelectionKeyboard(session.selectedGenres);
            messenger->editMessageWithKeyboard(session.chatId, cb.messageId, text, kb);
        }
        return;
    }

    if (cb.data == "genre:confirm" || cb.data == "genre:done") {
        if (session.selectedGenres.empty()) {
            if (auto messenger = ctx.getMessenger()) {
                messenger->sendMessage(session.chatId,
                    "⚠️ Пожалуйста, выберите хотя бы один любимый жанр перед продолжением.");
            }
            return;
        }
        ctx.saveUserGenres(session.userId, session.selectedGenres);
        ctx.transitionTo(session.userId, BotState::BrowsingRecommendations);
        return;
    }

    if (cb.data == "genre:cancel") {
        ctx.transitionTo(session.userId, BotState::Idle);
    }
}

std::string GenreSelectionState::buildStatusText(const UserSession& session, const std::string& prefix) {
    std::ostringstream oss;
    if (!prefix.empty()) {
        oss << prefix << "\n";
    }

    oss << "Текущие выбранные жанры: ";
    if (session.selectedGenres.empty()) {
        oss << "(пока ничего не выбрано)\n";
    } else {
        for (size_t i = 0; i < session.selectedGenres.size(); ++i) {
            oss << session.selectedGenres[i];
            if (i + 1 < session.selectedGenres.size()) {
                oss << ", ";
            }
        }
        oss << "\n";
    }
    oss << "Нажмите на жанр для добавления/удаления или «✅ Подтвердить выбор»:";
    return oss.str();
}

void GenreSelectionState::sendStatusMessage(IDialogStateContext& ctx, const UserSession& session, const std::string& prefix) {
    auto messenger = ctx.getMessenger();
    if (!messenger) {
        return;
    }

    std::string text = buildStatusText(session, prefix);
    messenger->sendMessageWithKeyboard(
        session.chatId,
        text,
        KeyboardFactory::createGenreSelectionKeyboard(session.selectedGenres));
}

} // namespace bot
