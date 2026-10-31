#include "bot/CommandDispatcher.h"
#include "bot/KeyboardFactory.h"

#include <sstream>
#include <utility>

/**
 * @file CommandDispatcher.cpp
 * @brief Реализация диспетчера команд CommandDispatcher.
 */

namespace bot {

CommandDispatcher::CommandDispatcher(IDialogStateContext& context, std::shared_ptr<IBotMessenger> messenger)
    : context_(context),
      messenger_(std::move(messenger)) {
}

std::string CommandDispatcher::extractCommand(const std::string& text) {
    if (text.empty() || text[0] != '/') {
        return "";
    }
    auto spacePos = text.find(' ');
    std::string cmd = (spacePos == std::string::npos) ? text : text.substr(0, spacePos);
    auto atPos = cmd.find('@');
    if (atPos != std::string::npos) {
        cmd = cmd.substr(0, atPos);
    }
    return cmd;
}

bool CommandDispatcher::dispatch(const IncomingMessage& msg) {
    std::string cmd = extractCommand(msg.text);
    if (cmd.empty()) return false;

    auto& session = context_.getSession(msg.userId, msg.chatId, msg.username);
    if (cmd == "/start") handleStart(session);
    else if (cmd == "/recommend") handleRecommend(session);
    else if (cmd == "/genres") handleGenres(session);
    else if (cmd == "/history") handleHistory(session);
    else if (cmd == "/stats") handleStats(session);
    else if (cmd == "/help") handleHelp(session);
    else if (cmd == "/menu") context_.transitionTo(session.userId, BotState::Idle);
    else return false;
    return true;
}

void CommandDispatcher::handleStart(UserSession& session) {
    context_.transitionTo(session.userId, BotState::SelectingGenres);
}

void CommandDispatcher::handleRecommend(UserSession& session) {
    if (!session.hasSelectedGenres()) {
        if (messenger_) {
            messenger_->sendMessage(
                session.chatId,
                "⚠️ Для подбора рекомендаций сначала выберите хотя бы один любимый жанр.");
        }
        context_.transitionTo(session.userId, BotState::SelectingGenres);
        return;
    }
    context_.transitionTo(session.userId, BotState::BrowsingRecommendations);
}

void CommandDispatcher::handleGenres(UserSession& session) {
    context_.transitionTo(session.userId, BotState::SelectingGenres);
}

static std::string formatRatingEntries(const UserSession& session, IDialogStateContext& ctx) {
    std::ostringstream oss;
    oss << "📜 Ваша история оценок:\n";
    for (const auto& [movieId, score] : session.getMovieRatings()) {
        auto movieOpt = ctx.getMovieById(movieId);
        std::string title = movieOpt ? movieOpt->title : ("Фильм #" + std::to_string(movieId));
        oss << "• " << title << " — " << score << "/10\n";
    }
    return oss.str();
}

std::string CommandDispatcher::formatHistoryText(const UserSession& session, const std::string& strategy) const {
    if (!session.hasRatings()) {
        std::string text = "Ваша история просмотров и оценок пока пуста.";
        return strategy.empty() ? text : (text + "\n🎯 Активный режим рекомендаций: " + strategy);
    }
    std::string text = formatRatingEntries(session, context_);
    if (!strategy.empty()) text += "\n🎯 Активный режим рекомендаций: " + strategy + "\n";
    return text + "\nИспользуйте меню ниже для дальнейших действий:";
}

void CommandDispatcher::handleHistory(const UserSession& session) {
    if (!messenger_) return;
    std::string text = formatHistoryText(session, context_.getActiveStrategyName(session.userId));
    messenger_->sendMessageWithKeyboard(session.chatId, text, KeyboardFactory::createMainMenuKeyboard());
}

void CommandDispatcher::handleStats(const UserSession& session) {
    if (!messenger_) return;
    std::string report = context_.getMetricsReport();
    if (report.empty()) report = "📊 Метрики пока не собраны (коллектор не инициализирован).";
    report += "\n\nИспользуйте меню ниже для дальнейших действий:";
    messenger_->sendMessageWithKeyboard(session.chatId, report, KeyboardFactory::createMainMenuKeyboard());
}

std::string CommandDispatcher::buildHelpText() {
    return "ℹ️ Справка по командам и возможностям бота:\n\n"
           "📋 Команды:\n"
           "• /start — Запуск онбординга | • /recommend — Подбор рекомендаций\n"
           "• /genres — Настройка жанров | • /history — История оценок\n"
           "• /similar — Поиск похожих | • /stats — Аналитика | • /help — Справка\n\n"
           "🎬 Кнопки под карточкой фильма:\n"
           "• 👍 Понравился (10/10) | • 👎 Не понравился (1/10)\n"
           "• ⭐ Оценить (1-10) | • ⏭ Следующий | • 🔍 Похожие\n\n"
           "🧠 Стратегии рекомендаций:\n"
           "1. Cold-Start / Popularity — топовые фильмы каталога в выбранных жанрах.\n"
           "2. Content-Based Filtering — подбор по сходству жанров, режиссера и эпохи.\n\n"
           "Используйте кнопки меню ниже для быстрой навигации:";
}

void CommandDispatcher::handleHelp(const UserSession& session) {
    if (!messenger_) {
        return;
    }
    messenger_->sendMessageWithKeyboard(session.chatId, buildHelpText(), KeyboardFactory::createMainMenuKeyboard());
}

} // namespace bot
