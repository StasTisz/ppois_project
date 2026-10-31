#pragma once

#include <cstddef>
#include <string>
#include <vector>

/**
 * @file InlineKeyboard.h
 * @brief Модели кнопок и структуры Inline-клавиатуры для сообщений Telegram-бота.
 */

namespace bot {

/**
 * @struct InlineButton
 * @brief Описание отдельной кнопки во встроенной (inline) клавиатуре.
 */
struct InlineButton {
    std::string text;         ///< Отображаемый текст на кнопке.
    std::string callbackData; ///< Полезная нагрузка (callback_data), возвращаемая боту при нажатии.
};

/// Псевдоним для ряда кнопок клавиатуры.
using InlineKeyboardRow = std::vector<InlineButton>;

/**
 * @struct InlineKeyboard
 * @brief Разметка Inline-клавиатуры, содержащая массив строк с кнопками.
 */
struct InlineKeyboard {
    std::vector<InlineKeyboardRow> rows; ///< Список строк кнопок клавиатуры.

    /**
     * @brief Проверяет, пуста ли клавиатура (не содержит ни одной кнопки).
     * @return true, если кнопок нет, иначе false.
     */
    [[nodiscard]] bool empty() const {
        for (const auto& row : rows) {
            if (!row.empty()) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Возвращает общее количество кнопок во всех рядах клавиатуры.
     * @return Число кнопок.
     */
    [[nodiscard]] size_t buttonCount() const {
        size_t count = 0;
        for (const auto& row : rows) {
            count += row.size();
        }
        return count;
    }

    /**
     * @brief Ищет кнопку по точному значению callbackData или по префиксу с двоеточием.
     * @param callbackData Значение callback-данных для поиска.
     * @return Указатель на найденную кнопку, либо nullptr, если кнопка не найдена.
     */
    [[nodiscard]] const InlineButton* findByCallback(const std::string& callbackData) const {
        for (const auto& row : rows) {
            for (const auto& btn : row) {
                if (btn.callbackData == callbackData ||
                    btn.callbackData.rfind(callbackData + ":", 0) == 0) {
                    return &btn;
                }
            }
        }
        return nullptr;
    }
};

} // namespace bot
