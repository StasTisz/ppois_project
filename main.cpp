#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

// Наша основная сущность. Ничего лишнего.
class MediaItem {
public:
    int id;
    std::string title;
    std::string category;
    std::string description;
    std::vector<float> embedding; // Массив для будущей магии нейронок

    // Конструктор на списках инициализации — стильно и быстро
    MediaItem(int id, std::string t, std::string cat, std::string desc)
        : id(id), title(std::move(t)), category(std::move(cat)), description(std::move(desc)) {}
};

// Тот самый менеджер, который будет грузить наш CSV
class DatabaseManager {
private:
    std::vector<MediaItem> database;

public:
    // Возвращаем bool, чтобы main понял, норм всё или мы откисли
    bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename);

        if (!file.is_open()) {
            std::cerr << "[ERROR] Бро, файл " << filename << " не найден! Проверь пути в CLion." << std::endl;
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue; // Скипаем пустые строки, чтобы не крашнуться

            std::stringstream ss(line);
            std::string id_str, title, category, description;

            // Парсим строго по точке с запятой
            std::getline(ss, id_str, ';');
            std::getline(ss, title, ';');
            std::getline(ss, category, ';');
            std::getline(ss, description, ';');

            try {
                int id = std::stoi(id_str);
                // emplace_back работает быстрее push_back, так как конструирует объект сразу на месте
                database.emplace_back(id, title, category, description);
            } catch (const std::exception& e) {
                std::cerr << "[WARN] Скипнули кривую строку, id не читается: " << line << std::endl;
            }
        }

        std::cout << "[SUCCESS] База в памяти! Загружено тайтлов: " << database.size() << std::endl;
        return true;
    }

    // Геттер, чтобы отдавать базу наружу, но только для чтения (const)
    const std::vector<MediaItem>& getItems() const {
        return database;
    }
};

int main() {
    DatabaseManager dbManager;

    // Пытаемся загрузить базу
    if (dbManager.loadFromFile("database.csv")) {
        // Если всё гуд, просто флексим тем, что мы загрузили
        std::cout << "--- Что у нас есть ---" << std::endl;
        for (const auto& item : dbManager.getItems()) {
            std::cout << "🔥 " << item.title << " [" << item.category << "]" << std::endl;
        }
    }

    return 0;
}