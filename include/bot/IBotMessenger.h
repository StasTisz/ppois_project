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
 */
class IBotMessenger {
public:
    virtual ~IBotMessenger() = default;

    virtual void sendMessage(int64_t chatId, const std::string& text) = 0;

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
     * @param chatId Уникальный идентификатор чата.
     * @param photoUrlOrFileId URL-ссылка на фото либо уже кэшированный Telegram file_id.
     * @param caption Текстовая подпись к фото.
     * @param keyboard Структура кнопок inline-клавиатуры.
     * @param fileIdOut Опциональный указатель на строку для сохранения полученного от Telegram file_id.
     * @return true при успешной отправке фото, false при ошибке или fallback.
     */
    virtual bool sendPhotoWithKeyboard(
        int64_t chatId,
        const std::string& photoUrlOrFileId,
        const std::string& caption,
        const InlineKeyboard& keyboard) {
        (void)photoUrlOrFileId;
        sendMessageWithKeyboard(chatId, caption, keyboard);
        return true;
    }

    virtual bool sendPhotoWithKeyboard(
        int64_t chatId,
        const std::string& photoUrlOrFileId,
        const std::string& caption,
        const InlineKeyboard& keyboard,
        std::string* fileIdOut) {
        (void)fileIdOut;
        return sendPhotoWithKeyboard(chatId, photoUrlOrFileId, caption, keyboard);
    }

    virtual void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") = 0;
};

} // namespace bot
