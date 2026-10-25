#pragma once

#include "bot/InlineKeyboard.h"

#include <cstdint>
#include <string>

/**
 * @file IBotMessenger.h
 * @brief Абстрактный интерфейс отправки сообщений и обработки callback-запросов бота.
 */

namespace bot {

/**
 * @class IBotMessenger
 * @brief Контракт, изолирующий логику бота и UI от конкретной библиотеки сетевого взаимодействия.
 *
 * Предоставляет методы для отправки текстовых сообщений и подтверждения нажатия inline-кнопок.
 * Использование данного интерфейса позволяет подменять реальный сетевой адаптер (`TgBotMessenger`)
 * на тестовый дублер (`MockBotMessenger`) в юнит-тестах.
 */
class IBotMessenger {
public:
    /**
     * @brief Виртуальный деструктор для корректного полиморфного удаления реализаций.
     */
    virtual ~IBotMessenger() = default;

    /**
     * @brief Отправляет текстовое сообщение в указанный чат Telegram.
     * @param chatId Уникальный идентификатор целевого чата или диалога.
     * @param text Текст отправляемого сообщения в кодировке UTF-8.
     */
    virtual void sendMessage(int64_t chatId, const std::string& text) = 0;

    /**
     * @brief Отправляет текстовое сообщение с inline-клавиатурой в чат Telegram.
     * @param chatId Уникальный идентификатор целевого чата или диалога.
     * @param text Текст отправляемого сообщения в кодировке UTF-8.
     * @param keyboard Структура кнопок inline-клавиатуры.
     */
    virtual void sendMessageWithKeyboard(int64_t chatId, const std::string& text, const InlineKeyboard& keyboard) {
        (void)keyboard;
        sendMessage(chatId, text);
    }

    /**
     * @brief Редактирует существующее текстовое сообщение с inline-клавиатурой в чате Telegram.
     *
     * Реализация по умолчанию выполняет fallback на sendMessageWithKeyboard,
     * обеспечивая совместимость с существующими тестовыми spy-мессенджерами.
     *
     * @param chatId Уникальный идентификатор целевого чата или диалога.
     * @param messageId Идентификатор редактируемого сообщения.
     * @param text Новый текст сообщения в кодировке UTF-8.
     * @param keyboard Новая структура кнопок inline-клавиатуры.
     */
    virtual void editMessageWithKeyboard(
        int64_t chatId,
        int32_t messageId,
        const std::string& text,
        const InlineKeyboard& keyboard) {
        (void)messageId;
        sendMessageWithKeyboard(chatId, text, keyboard);
    }

    /**
     * @brief Отправляет фото (постер) с подписью и inline-клавиатурой в чат Telegram.
     *
     * Реализация по умолчанию выполняет fallback на sendMessageWithKeyboard,
     * обеспечивая совместимость с тестовыми дублерами без графического вывода.
     *
     * @param chatId Уникальный идентификатор целевого чата или диалога.
     * @param photoUrl URL-ссылка на изображение постера.
     * @param caption Текстовая подпись к фото.
     * @param keyboard Структура кнопок inline-клавиатуры.
     * @return true при успешной отправке фото, false при ошибке или fallback.
     */
    virtual bool sendPhotoWithKeyboard(
        int64_t chatId,
        const std::string& photoUrl,
        const std::string& caption,
        const InlineKeyboard& keyboard) {
        (void)photoUrl;
        sendMessageWithKeyboard(chatId, caption, keyboard);
        return true;
    }

    /**
     * @brief Отправляет подтверждение на входящий callback-запрос от inline-кнопки.
     *
     * Вызов этого метода прекращает анимацию загрузки ("часики") на клиенте пользователя Telegram.
     * Опционально может выводить всплывающее уведомление (toast/alert).
     *
     * @param callbackId Идентификатор подтверждаемого callback-запроса.
     * @param text Опциональный текст уведомления, отображаемый пользователю.
     */
    virtual void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") = 0;
};

} // namespace bot
