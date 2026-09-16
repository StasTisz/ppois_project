cmake_minimum_required(VERSION 3.14)
project(StudentMediaBot CXX C)

# Ставим современный стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- КРОССПЛАТФОРМЕННАЯ МАГИЯ ---

if (APPLE)
    message(STATUS "🍎 Обнаружен Mac! Включаем поддержку Metal для Apple Silicon...")
    # Принудительно включаем Metal, чтобы модель летала на GPU макбука
    set(LLAMA_METAL ON CACHE BOOL "" FORCE)
elseif (WIN32)
    message(STATUS "🪟 Обнаружена Windows! Собираем базовую версию...")
    # По умолчанию на Винде соберется CPU-версия.
    # Если у напарника видеокарта Nvidia, он потом сможет поменять тут на LLAMA_CUDA
endif()

# Подключаем папку с llama.cpp как часть нашего проекта
add_subdirectory(llama.cpp)

# Наш главный исполняемый файл
add_executable(MediaBot main.cpp)

# Линкуем библиотеки llama.cpp к нашему проекту
target_link_libraries(MediaBot PRIVATE llama ggml)