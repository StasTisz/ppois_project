#include "bot/states/GenreSelectionState.h"
#include "bot/KeyboardFactory.h"
#include "core/GenreMask.h"

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
        ctx.transitionTo(session.userId, BotState::BrowsingRecommendations);
        return;
    }

    if (msg.text == "/cancel" || msg.text == "/menu") {
        ctx.transitionTo(session.userId, BotState::Idle);
        return;
    }

    // Если передан текст названия жанра
    if (!msg.text.empty() && msg.text[0] != '/') {
        core::GenreBit bit = core::stringToGenreBit(msg.text);
        if (bit == core::GenreBit::Unknown) {
            if (messenger) {
                messenger->sendMessage(session.chatId,
                    "⚠️ Не удалось распознать жанр: \"" + msg.text + "\".\n"
                    "Пожалуйста, используйте кнопки меню ниже для выбора жанров.");
            }
            sendStatusMessage(ctx, session);
            return;
        }

        bool added = session.toggleGenre(msg.text);
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
        bool added = session.toggleGenre(genre);
        std::string notice = added
            ? ("✅ Жанр '" + genre + "' добавлен в предпочтения.\n")
            : ("❌ Жанр '" + genre + "' удален из предпочтений.\n");
        sendStatusMessage(ctx, session, notice);
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
        ctx.transitionTo(session.userId, BotState::BrowsingRecommendations);
        return;
    }

    if (cb.data == "genre:cancel") {
        ctx.transitionTo(session.userId, BotState::Idle);
    }
}

void GenreSelectionState::sendStatusMessage(IDialogStateContext& ctx, const UserSession& session, const std::string& prefix) {
    auto messenger = ctx.getMessenger();
    if (!messenger) {
        return;
    }

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

    oss << "\nОтправьте название жанра текстом или выберите кнопками ниже, затем нажмите /done или «✔️ Готово» (/cancel для отмены):";
    messenger->sendMessageWithKeyboard(
        session.chatId,
        oss.str(),
        KeyboardFactory::createGenreSelectionKeyboard(session.selectedGenres));
}

} // namespace bot
