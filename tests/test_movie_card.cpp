#include <gtest/gtest.h>

#include "bot/KeyboardFactory.h"
#include "bot/MovieCardFormatter.h"
#include "bot/NetworkErrorHandler.h"
#include "core/Movie.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * @file test_movie_card.cpp
 * @brief Модульные тесты для проверки MovieCardFormatter и NetworkErrorHandler.
 */

namespace {

/**
 * @brief Проверяет валидность UTF-8 кодировки строки.
 */
bool isValidUtf8(const std::string& str) {
    size_t i = 0;
    while (i < str.size()) {
        auto c = static_cast<unsigned char>(str[i]);
        if (c <= 0x7F) {
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= str.size() || (static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80) {
                return false;
            }
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= str.size() ||
                (static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(str[i + 2]) & 0xC0) != 0x80) {
                return false;
            }
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= str.size() ||
                (static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(str[i + 2]) & 0xC0) != 0x80 ||
                (static_cast<unsigned char>(str[i + 3]) & 0xC0) != 0x80) {
                return false;
            }
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

/**
 * @class MockMediaMessenger
 * @brief Тестовый дублер для проверки вызовов отправки фото и текстовых сообщений.
 */
class MockMediaMessenger : public bot::IBotMessenger {
public:
    struct PhotoCall {
        int64_t chatId{0};
        std::string photoUrl;
        std::string caption;
        bot::InlineKeyboard keyboard;
    };

    struct TextCall {
        int64_t chatId{0};
        std::string text;
        bot::InlineKeyboard keyboard;
    };

    bool photoSendResult{true};
    std::vector<PhotoCall> photoCalls;
    std::vector<TextCall> textCalls;

    void sendMessage(int64_t chatId, const std::string& text) override {
        textCalls.push_back({chatId, text, {}});
    }

    void sendMessageWithKeyboard(int64_t chatId, const std::string& text, const bot::InlineKeyboard& keyboard) override {
        textCalls.push_back({chatId, text, keyboard});
    }

    bool sendPhotoWithKeyboard(
        int64_t chatId, const std::string& photoUrl,
        const std::string& caption, const bot::InlineKeyboard& keyboard) override {
        photoCalls.push_back({chatId, photoUrl, caption, keyboard});
        return photoSendResult;
    }

    void answerCallbackQuery(const std::string& callbackId, const std::string& text = "") override {
        (void)callbackId;
        (void)text;
    }
};

} // namespace

/**
 * @test Проверяет полное форматирование карточки фильма со всеми полями.
 */
TEST(MovieCardFormatterTest, FormatsCompleteMovieCard) {
    core::Movie movie;
    movie.id = 1;
    movie.title = "Интерстеллар";
    movie.year = 2014;
    movie.genres = {"Sci-Fi", "Drama"};
    movie.director = "Кристофер Нолан";
    movie.description = "Путешествие исследователей сквозь червоточину.";
    movie.ratingAvg = 8.7;

    std::string card = bot::MovieCardFormatter::formatMovieCard(movie);

    EXPECT_NE(card.find("Интерстеллар (2014)"), std::string::npos);
    EXPECT_NE(card.find("8.7 / 10"), std::string::npos);
    EXPECT_NE(card.find("Sci-Fi, Drama"), std::string::npos);
    EXPECT_NE(card.find("Кристофер Нолан"), std::string::npos);
    EXPECT_NE(card.find("Путешествие исследователей"), std::string::npos);
    EXPECT_NE(card.find("Выберите реакцию"), std::string::npos);
}

/**
 * @test Проверяет форматирование фильма с отсутствующими опциональными полями.
 */
TEST(MovieCardFormatterTest, HandlesMissingOptionalFields) {
    core::Movie movie;
    movie.id = 2;
    movie.title = "Неизвестный фильм";
    movie.year = 0; // год не указан
    movie.genres = {}; // жанры не указаны
    movie.director = ""; // режиссер не указан
    movie.description = ""; // описания нет
    movie.ratingAvg = 7.0;

    std::string card = bot::MovieCardFormatter::formatMovieCard(movie);

    EXPECT_NE(card.find("🎬 Неизвестный фильм\n"), std::string::npos);
    EXPECT_EQ(card.find("(0)"), std::string::npos);
    EXPECT_NE(card.find("📁 Жанры: Не указаны"), std::string::npos);
    EXPECT_EQ(card.find("Режиссер:"), std::string::npos);
    EXPECT_EQ(card.find("Описание:"), std::string::npos);
}

/**
 * @test Проверяет корректное усечение длинного кириллического синопсиса под лимит Telegram (1024 байта).
 */
TEST(MovieCardFormatterTest, TruncatesLongSynopsisWithinTelegramCaptionLimit) {
    core::Movie movie;
    movie.id = 3;
    movie.title = "Очень длинная драма";
    movie.year = 2024;
    movie.genres = {"Drama"};
    movie.ratingAvg = 9.5;

    // Генерируем длинное кириллическое описание (> 2500 байт)
    std::string longDesc;
    for (int i = 0; i < 50; ++i) {
        longDesc += "Много текста о сложных человеческих взаимоотношениях и судьбе. ";
    }
    movie.description = longDesc;

    std::string card = bot::MovieCardFormatter::formatMovieCard(movie, bot::MovieCardFormatter::kMaxPhotoCaptionLength);

    EXPECT_LE(card.size(), bot::MovieCardFormatter::kMaxPhotoCaptionLength);
    EXPECT_TRUE(isValidUtf8(card));
    EXPECT_NE(card.find("..."), std::string::npos);
    EXPECT_NE(card.find("Выберите реакцию кнопками ниже:"), std::string::npos);
}

/**
 * @test Проверяет валидатор URL постеров.
 */
TEST(MovieCardFormatterTest, ValidatesPosterUrls) {
    core::Movie movie;

    movie.posterUrl = "https://image.tmdb.org/t/p/w500/poster.jpg";
    EXPECT_TRUE(bot::MovieCardFormatter::hasValidPosterUrl(movie));

    movie.posterUrl = "http://example.com/poster.png";
    EXPECT_TRUE(bot::MovieCardFormatter::hasValidPosterUrl(movie));

    movie.posterUrl = "";
    EXPECT_FALSE(bot::MovieCardFormatter::hasValidPosterUrl(movie));

    movie.posterUrl = "ftp://example.com/poster.jpg";
    EXPECT_FALSE(bot::MovieCardFormatter::hasValidPosterUrl(movie));

    movie.posterUrl = "not_a_valid_url";
    EXPECT_FALSE(bot::MovieCardFormatter::hasValidPosterUrl(movie));
}

/**
 * @test Проверяет, что при валидном URL вызывается отправка фото sendPhotoWithKeyboard.
 */
TEST(MovieCardFormatterTest, SendsPhotoWhenPosterUrlIsValid) {
    MockMediaMessenger mock;
    core::Movie movie;
    movie.id = 10;
    movie.title = "Начало";
    movie.year = 2010;
    movie.posterUrl = "https://image.tmdb.org/t/p/w500/inception.jpg";

    auto kb = bot::KeyboardFactory::createMovieReactionKeyboard();
    bot::MovieCardFormatter::sendMovieCard(mock, 12345, movie, kb);

    ASSERT_EQ(mock.photoCalls.size(), 1);
    EXPECT_EQ(mock.photoCalls[0].chatId, 12345);
    EXPECT_EQ(mock.photoCalls[0].photoUrl, movie.posterUrl);
    EXPECT_NE(mock.photoCalls[0].caption.find("Начало"), std::string::npos);
    EXPECT_FALSE(mock.photoCalls[0].keyboard.empty());
    EXPECT_TRUE(mock.textCalls.empty());
}

/**
 * @test Проверяет fallback на текстовое сообщение при пустом URL или ошибке загрузки фото.
 */
TEST(MovieCardFormatterTest, FallsBackToTextMessageWhenPosterIsEmptyOrFails) {
    MockMediaMessenger mock;
    auto kb = bot::KeyboardFactory::createMovieReactionKeyboard();

    // 1. Постер отсутствует (пустой URL) -> сразу отправка текстового сообщения
    core::Movie noPosterMovie;
    noPosterMovie.id = 11;
    noPosterMovie.title = "Фильм без постера";
    noPosterMovie.posterUrl = "";

    bot::MovieCardFormatter::sendMovieCard(mock, 12345, noPosterMovie, kb);
    EXPECT_TRUE(mock.photoCalls.empty());
    ASSERT_EQ(mock.textCalls.size(), 1);
    EXPECT_NE(mock.textCalls[0].text.find("Фильм без постера"), std::string::npos);

    // 2. Постер задан, но отправка завершилась ошибкой (например, 404 на сервере постеров) -> fallback на текст
    core::Movie failedPosterMovie;
    failedPosterMovie.id = 12;
    failedPosterMovie.title = "Фильм с битым постером";
    failedPosterMovie.posterUrl = "https://example.com/broken.jpg";

    mock.photoSendResult = false; // симулируем отказ sendPhoto
    bot::MovieCardFormatter::sendMovieCard(mock, 12345, failedPosterMovie, kb);

    EXPECT_EQ(mock.photoCalls.size(), 1);
    ASSERT_EQ(mock.textCalls.size(), 2);
    EXPECT_NE(mock.textCalls.back().text.find("Фильм с битым постером"), std::string::npos);
}

/**
 * @test Проверяет классификацию ошибок сети и логику повторов (executeWithRetry).
 */
TEST(NetworkErrorHandlerTest, ClassifiesErrorsAndRetriesTransientFailures) {
    using namespace bot;

    // Проверка классификатора
    EXPECT_EQ(NetworkErrorHandler::classifyError("Connection timed out after 5000 ms"), NetworkErrorType::Timeout);
    EXPECT_EQ(NetworkErrorHandler::classifyError("Too Many Requests: retry after 5"), NetworkErrorType::RateLimit);
    EXPECT_EQ(NetworkErrorHandler::classifyError("Bad Request: failed to get HTTP URL content"), NetworkErrorType::InvalidMediaUrl);
    EXPECT_EQ(NetworkErrorHandler::classifyError(""), NetworkErrorType::None);

    // Проверка executeWithRetry с временной ошибкой (Timeout)
    int attemptCount = 0;
    RetryConfig config;
    config.maxRetries = 2;
    config.retryDelayMs = 0;

    bool result = NetworkErrorHandler::executeWithRetry([&attemptCount]() {
        ++attemptCount;
        if (attemptCount < 3) {
            throw std::runtime_error("Socket read timed out");
        }
    }, config);

    EXPECT_TRUE(result);
    EXPECT_EQ(attemptCount, 3);

    // Проверка executeWithRetry с неретраябельной ошибкой (InvalidMediaUrl)
    int failAttemptCount = 0;
    bool failResult = NetworkErrorHandler::executeWithRetry([&failAttemptCount]() {
        ++failAttemptCount;
        throw std::runtime_error("Bad Request: wrong file identifier/HTTP URL specified");
    }, config);

    EXPECT_FALSE(failResult);
    EXPECT_EQ(failAttemptCount, 1); // не повторяет при InvalidMediaUrl
}
