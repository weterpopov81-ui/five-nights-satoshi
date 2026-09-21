#include "menu.h"
#include <cstdio>
#include <cstring>

const char* SAVE_FILE = "save.dat";

// ============================================================
// SAVE / LOAD
// ============================================================
void SaveGame(int night, double fuel)
{
    FILE* f = fopen(SAVE_FILE, "wb");
    if (f)
    {
        fwrite(&night, sizeof(int), 1, f);
        fwrite(&fuel, sizeof(double), 1, f);
        fclose(f);
    }
}

SaveData LoadGame()
{
    SaveData data = {1, 100.0, false};
    FILE* f = fopen(SAVE_FILE, "rb");
    if (f)
    {
        if (fread(&data.currentNight, sizeof(int), 1, f) == 1 &&
            fread(&data.fuel, sizeof(double), 1, f) == 1)
        {
            data.hasSave = true;
        }
        fclose(f);
    }
    return data;
}

void DeleteSave()
{
    remove(SAVE_FILE);
}

// ============================================================
// MENU BUTTON (поддержка мыши + тач)
// ============================================================
bool DrawMenuButton(const char* text, Rectangle rect, bool hover)
{
    Color bgColor = hover ? Color{80, 80, 120, 255} : Color{40, 40, 60, 255};
    Color textColor = hover ? YELLOW : WHITE;
    
    DrawRectangleRec(rect, bgColor);
    DrawRectangleLinesEx(rect, 2, hover ? YELLOW : GRAY);
    
    int fontSize = 28;
    int textWidth = MeasureText(text, fontSize);
    int textX = (int)rect.x + ((int)rect.width - textWidth) / 2;
    int textY = (int)rect.y + ((int)rect.height - fontSize) / 2;
    
    DrawText(text, textX, textY, fontSize, textColor);
    
    // Мышь
    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        return true;

    // Тач (Android)
    Vector2 touch = GetTouchPosition(0);
    if (touch.x >= 0 && CheckCollisionPointRec(touch, rect))
    {
        // Проверяем, что палец только что коснулся (простая версия)
        static Vector2 lastTouch = {-1, -1};
        if (lastTouch.x < 0)
        {
            lastTouch = touch;
            return true;
        }
    }
    else
    {
        // Сброс, когда палец убран
        // (упрощённо)
    }

    return false;
}

// Более надёжная проверка тача
bool IsButtonPressed(Rectangle rect)
{
    // Мышь
    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        return true;

    // Тач
    for (int i = 0; i < GetTouchPointCount(); i++)
    {
        Vector2 touch = GetTouchPosition(i);
        if (CheckCollisionPointRec(touch, rect))
            return true;
    }
    return false;
}

// ============================================================
// MAIN MENU
// ============================================================
MenuChoice ShowMainMenu()
{
    const int W = 1280;
    const int H = 720;
    
    InitWindow(W, H, "Пять ночей с бобром");
    SetTargetFPS(60);
    
    SaveData save = LoadGame();
    
    // Кнопки
    Rectangle btnNew  = {W/2.0f - 150, 300, 300, 70};
    Rectangle btnCont = {W/2.0f - 150, 390, 300, 70};
    Rectangle btnExit = {W/2.0f - 150, 480, 300, 70};
    
    MenuChoice choice = CHOICE_NONE;
    
    while (!WindowShouldClose() && choice == CHOICE_NONE)
    {
        Vector2 mouse = GetMousePosition();
        bool hoverNew  = CheckCollisionPointRec(mouse, btnNew);
        bool hoverCont = save.hasSave && CheckCollisionPointRec(mouse, btnCont);
        bool hoverExit = CheckCollisionPointRec(mouse, btnExit);

        // Также подсветка при таче
        Vector2 touch = GetTouchPosition(0);
        if (touch.x >= 0)
        {
            if (CheckCollisionPointRec(touch, btnNew))  hoverNew = true;
            if (save.hasSave && CheckCollisionPointRec(touch, btnCont)) hoverCont = true;
            if (CheckCollisionPointRec(touch, btnExit)) hoverExit = true;
        }
        
        BeginDrawing();
        ClearBackground(BLACK);
        
        // Фон
        DrawRectangle(0, 0, W, H, Color{20, 20, 30, 255});
        
        // Заголовок
        DrawText("ПЯТЬ НОЧЕЙ", W/2 - 180, 90, 55, RED);
        DrawText("С БОБРОМ", W/2 - 140, 155, 50, RED);
        
        // Подзаголовок
        DrawText("Выжить до 06:00", W/2 - 100, 230, 22, GRAY);
        
        // Кнопки
        DrawMenuButton("НОВАЯ ИГРА", btnNew, hoverNew);
        
        if (save.hasSave)
        {
            char contText[64];
            snprintf(contText, sizeof(contText), "ПРОДОЛЖИТЬ (Ночь %d)", save.currentNight);
            DrawMenuButton(contText, btnCont, hoverCont);
        }
        else
        {
            DrawRectangleRec(btnCont, Color{30, 30, 30, 255});
            DrawRectangleLinesEx(btnCont, 2, DARKGRAY);
            int tw = MeasureText("ПРОДОЛЖИТЬ (нет сохранения)", 22);
            DrawText("ПРОДОЛЖИТЬ (нет сохранения)", W/2 - tw/2, 412, 22, DARKGRAY);
        }
        
        DrawMenuButton("ВЫХОД", btnExit, hoverExit);
        
        // Подсказка
#if defined(PLATFORM_ANDROID)
        DrawText("Касайся кнопок пальцем", 20, H - 40, 20, GRAY);
#else
        DrawText("Используй мышь", 20, H - 40, 20, GRAY);
#endif
        
        // Обработка нажатий
        if (IsButtonPressed(btnNew))
            choice = CHOICE_NEW_GAME;
        else if (save.hasSave && IsButtonPressed(btnCont))
            choice = CHOICE_CONTINUE;
        else if (IsButtonPressed(btnExit))
            choice = CHOICE_EXIT;
        
        EndDrawing();
    }
    
    CloseWindow();
    return choice;
}