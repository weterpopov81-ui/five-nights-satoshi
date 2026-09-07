#ifndef MENU_H
#define MENU_H

#include "raylib.h"

// Состояния меню
enum MenuState {
    MENU_MAIN,        // Главное меню
    MENU_PLAYING,     // Игра идёт
    MENU_PAUSED       // Пауза
};

// Результат выбора в меню
enum MenuChoice {
    CHOICE_NONE,      // Ничего не выбрано
    CHOICE_NEW_GAME,  // Новая игра
    CHOICE_CONTINUE,  // Продолжить
    CHOICE_EXIT       // Выход
};

// Данные сохранения
struct SaveData {
    int currentNight;
    double fuel;
    bool hasSave;
};

// Показать главное меню. Возвращает выбор игрока.
MenuChoice ShowMainMenu();

// Сохранить прогресс
void SaveGame(int night, double fuel);

// Загрузить прогресс
SaveData LoadGame();

// Удалить сохранение
void DeleteSave();

#endif