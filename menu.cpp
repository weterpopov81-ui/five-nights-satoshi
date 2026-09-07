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
// MENU BUTTON
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
    
    Vector2 mouse = GetMousePosition();
    return CheckCollisionPointRec(mouse, rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// ============================================================
// MAIN MENU
// ============================================================
MenuChoice ShowMainMenu()
{
    const int W = 1280;
    const int H = 720;
    
    InitWindow(W, H, "FIVE NIGHTS WITH SATOSHI");
    SetTargetFPS(60);
    
    SaveData save = LoadGame();
    
    // Buttons
    Rectangle btnNew = {W/2 - 150, 300, 300, 60};
    Rectangle btnCont = {W/2 - 150, 380, 300, 60};
    Rectangle btnExit = {W/2 - 150, 460, 300, 60};
    
    MenuChoice choice = CHOICE_NONE;
    
    while (!WindowShouldClose() && choice == CHOICE_NONE)
    {
        Vector2 mouse = GetMousePosition();
        bool hoverNew = CheckCollisionPointRec(mouse, btnNew);
        bool hoverCont = save.hasSave && CheckCollisionPointRec(mouse, btnCont);
        bool hoverExit = CheckCollisionPointRec(mouse, btnExit);
        
        BeginDrawing();
        ClearBackground(BLACK);
        
        // Dark background
        DrawRectangle(0, 0, W, H, Color{20, 20, 30, 255});
        
        // Title
        DrawText("FIVE NIGHTS", W/2 - 200, 100, 60, RED);
        DrawText("WITH SATOSHI", W/2 - 200, 170, 50, RED);
        
        // Subtitle
        DrawText("Survive until 06:00", W/2 - 120, 230, 20, GRAY);
        
        // Buttons
        DrawMenuButton("NEW GAME", btnNew, hoverNew);
        
        if (save.hasSave)
        {
            char contText[64];
            std::snprintf(contText, sizeof(contText), 
                         "CONTINUE (Night %d)", save.currentNight);
            DrawMenuButton(contText, btnCont, hoverCont);
        }
        else
        {
            DrawRectangleRec(btnCont, Color{30, 30, 30, 255});
            DrawRectangleLinesEx(btnCont, 2, DARKGRAY);
            int tw = MeasureText("CONTINUE (no save)", 24);
            DrawText("CONTINUE (no save)", 
                     W/2 - tw/2, 405, 24, DARKGRAY);
        }
        
        DrawMenuButton("EXIT", btnExit, hoverExit);
        
        // Hint
        DrawText("Use mouse to select", 20, H - 30, 18, GRAY);
        
        // Handle clicks
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (hoverNew) choice = CHOICE_NEW_GAME;
            else if (hoverCont && save.hasSave) choice = CHOICE_CONTINUE;
            else if (hoverExit) choice = CHOICE_EXIT;
        }
        
        EndDrawing();
    }
    
    CloseWindow();
    return choice;
}