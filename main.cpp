#include "menu.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>

const char* SCENE_FILE = "Untitled.glb";
const double GAME_LENGTH = 8.0 * 60.0;
Vector3 clockPosition = {0.000f, 2.255f, -0.400f};

double gameTime = 0.0;
bool mainPower = true;
bool lightOn = true;
bool generatorOn = false;
bool gpuNoise = false;
bool doorClosed = false;
bool gameOver = false;
bool finished = false;
bool nightComplete = false;
char gameOverReason[256] = "";
int cameraYaw = 0;

double fuel = 100.0;
const double MAX_FUEL = 100.0;
double fuelConsumptionRate = 0.5;

double gpuTemperature = 25.0;
const double MAX_TEMP = 95.0;
const double MIN_TEMP = 20.0;

int currentNight = 1;
const int maxNights = 5;

double nextLightEvent = 30.0;
double nextGpuEvent = 40.0;
double lightOffDuration = 15.0;
double gpuNoiseDuration = 10.0;
double lightOffTimer = 0.0;
double gpuNoiseTimer = 0.0;
double satoshiTimer = 0.0;
bool lightEventActive = false;
bool gpuEventActive = false;

Music glitchSound;
Music fanNoiseSound;
Music ambientSound;
bool glitchLoaded = false;
bool fanLoaded = false;
bool ambientLoaded = false;

const char* ambientFiles[] = {
    "quiet-rain.mp3",
    "interference-noisy-close-active.mp3"
};
const int ambientCount = 2;
int currentAmbient = -1;

bool FindClockName(const char* json, size_t jsonSize)
{
    const char* name = "\"name\"";
    for (size_t i = 0; i + 20 < jsonSize; i++)
    {
        if (std::memcmp(json + i, name, 6) == 0)
        {
            const char* p = json + i + 6;
            while ((size_t)(p - json) < jsonSize &&
                   (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':'))
                p++;
            if (*p != '"') continue;
            p++;
            if (std::strncmp(p, "Clock", 5) == 0) return true;
        }
    }
    return false;
}

bool CheckGLBForClock(const char* filename)
{
    FILE* file = std::fopen(filename, "rb");
    if (!file) return false;
    unsigned int magic = 0, version = 0, totalLength = 0;
    if (std::fread(&magic, 4, 1, file) != 1 ||
        std::fread(&version, 4, 1, file) != 1 ||
        std::fread(&totalLength, 4, 1, file) != 1)
    { std::fclose(file); return false; }
    if (magic != 0x46546C67) { std::fclose(file); return false; }
    unsigned int jsonLength = 0, jsonType = 0;
    if (std::fread(&jsonLength, 4, 1, file) != 1 ||
        std::fread(&jsonType, 4, 1, file) != 1)
    { std::fclose(file); return false; }
    if (jsonType != 0x4E4F534A) { std::fclose(file); return false; }
    char* json = new char[jsonLength + 1];
    if (std::fread(json, 1, jsonLength, file) != jsonLength)
    { delete[] json; std::fclose(file); return false; }
    json[jsonLength] = '\0';
    bool found = FindClockName(json, jsonLength);
    delete[] json;
    std::fclose(file);
    return found;
}

void FormatGameTime(double time, char* buf, size_t bufSize)
{
    int gameHours = (int)(time / 60.0);
    int currentHour = (22 + gameHours) % 24;
    std::snprintf(buf, bufSize, "%02d", currentHour);
}

void ScheduleNextLightEvent()
{
    int baseLight = 30 - (currentNight * 3);
    if (baseLight < 15) baseLight = 15;
    nextLightEvent = baseLight + (rand() % 20);
}

void ScheduleNextGpuEvent()
{
    int baseGpu = 40 - (currentNight * 4);
    if (baseGpu < 20) baseGpu = 20;
    nextGpuEvent = baseGpu + (rand() % 30);
}

void ResetGame()
{
    mainPower = true;
    lightOn = true;
    generatorOn = false;
    gpuNoise = false;
    doorClosed = false;
    gameOver = false;
    finished = false;
    nightComplete = false;
    gameOverReason[0] = '\0';
    cameraYaw = 0;
    gpuTemperature = 25.0;
    lightEventActive = false;
    gpuEventActive = false;
    lightOffTimer = 0.0;
    gpuNoiseTimer = 0.0;
    satoshiTimer = 0.0;
    lightOffDuration = 15.0 - (currentNight - 1) * 2.0;
    gpuNoiseDuration = 10.0 - (currentNight - 1) * 1.5;
    if (lightOffDuration < 6.0) lightOffDuration = 6.0;
    if (gpuNoiseDuration < 4.0) gpuNoiseDuration = 4.0;
    ScheduleNextLightEvent();
    ScheduleNextGpuEvent();
}

void SetGameOver(const char* reason)
{
    gameOver = true;
    std::snprintf(gameOverReason, sizeof(gameOverReason), "%s", reason);
}

void PlayRandomAmbient()
{
    if (ambientLoaded)
    {
        StopMusicStream(ambientSound);
        UnloadMusicStream(ambientSound);
        ambientLoaded = false;
    }
    int newIndex = rand() % ambientCount;
    if (ambientCount > 1 && newIndex == currentAmbient)
        newIndex = (newIndex + 1) % ambientCount;
    currentAmbient = newIndex;
    if (FileExists(ambientFiles[newIndex]))
    {
        ambientSound = LoadMusicStream(ambientFiles[newIndex]);
        SetMusicVolume(ambientSound, 0.3f);
        ambientSound.looping = true;  // ИСПРАВЛЕНО!
        PlayMusicStream(ambientSound);
        ambientLoaded = true;
    }
}

void StopAmbient()
{
    if (ambientLoaded)
    {
        StopMusicStream(ambientSound);
        ambientLoaded = false;
    }
}

void ResumeAmbient()
{
    if (!ambientLoaded && currentAmbient >= 0)
    {
        if (FileExists(ambientFiles[currentAmbient]))
        {
            ambientSound = LoadMusicStream(ambientFiles[currentAmbient]);
            SetMusicVolume(ambientSound, 0.3f);
            ambientSound.looping = true;  // ИСПРАВЛЕНО!
            PlayMusicStream(ambientSound);
            ambientLoaded = true;
        }
    }
}

int main()
{
    const int SCREEN_WIDTH = 1280;
    const int SCREEN_HEIGHT = 720;
    srand((unsigned int)time(nullptr));

    MenuChoice choice = ShowMainMenu();
    if (choice == CHOICE_EXIT) return 0;

    if (choice == CHOICE_NEW_GAME)
    {
        currentNight = 1;
        fuel = MAX_FUEL;
        DeleteSave();
    }
    else if (choice == CHOICE_CONTINUE)
    {
        SaveData save = LoadGame();
        currentNight = save.currentNight;
        fuel = save.fuel;
    }

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "FIVE NIGHTS WITH SATOSHI");
    InitAudioDevice();
    SetTargetFPS(60);

    if (!FileExists(SCENE_FILE))
    {
        while (!WindowShouldClose())
        {
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("ERROR: Untitled.glb not found!", 50, 50, 30, RED);
            DrawText("Put Untitled.glb next to game.exe", 50, 100, 24, WHITE);
            EndDrawing();
        }
        CloseAudioDevice();
        CloseWindow();
        return 1;
    }

    bool clockExists = CheckGLBForClock(SCENE_FILE);
    Model scene = LoadModel(SCENE_FILE);

    if (scene.meshCount <= 0)
    {
        while (!WindowShouldClose())
        {
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("ERROR: GLB could not be loaded!", 40, 40, 30, RED);
            EndDrawing();
        }
        CloseAudioDevice();
        CloseWindow();
        return 1;
    }

    PlayRandomAmbient();

    if (FileExists("glitch_sound.mp3"))
    {
        glitchSound = LoadMusicStream("glitch_sound.mp3");
        SetMusicVolume(glitchSound, 0.7f);
        glitchLoaded = true;
    }
    if (FileExists("fan_noise.mp3"))
    {
        fanNoiseSound = LoadMusicStream("fan_noise.mp3");
        SetMusicVolume(fanNoiseSound, 0.5f);
        fanLoaded = true;
    }

    Camera3D camera = {};
    camera.position = Vector3{0.0f, 1.5f, 4.0f};
    camera.target = Vector3{0.0f, 1.2f, 0.0f};
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    ResetGame();

    while (!WindowShouldClose())
    {
        double delta = GetFrameTime();

        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))
            if (cameraYaw > -1) cameraYaw--;
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT))
            if (cameraYaw < 1) cameraYaw++;

        float yawOffset = cameraYaw * 2.0f;
        camera.position.x = yawOffset * 0.5f;
        camera.target.x = yawOffset * 1.5f;

        if (IsKeyPressed(KEY_F) && cameraYaw == -1 && !gameOver && !finished && !nightComplete)
        {
            doorClosed = !doorClosed;
            if (gpuNoise && doorClosed)
            {
                gpuNoise = false;
                gpuEventActive = false;
                gpuNoiseTimer = 0.0;
                ScheduleNextGpuEvent();
                if (fanLoaded) StopMusicStream(fanNoiseSound);
                ResumeAmbient();
            }
        }

        if (IsKeyPressed(KEY_G) && cameraYaw == 1 && !gameOver && !finished && !nightComplete)
            generatorOn = !generatorOn;

        if (IsKeyPressed(KEY_SPACE))
        {
            if (gameOver)
            {
                DeleteSave();
                gameTime = 0.0;
                fuel = MAX_FUEL;
                ResetGame();
            }
            else if (finished)
            {
                if (currentNight < maxNights)
                {
                    currentNight++;
                    gameTime = 0.0;
                    double savedFuel = fuel;
                    ResetGame();
                    fuel = savedFuel;
                    PlayRandomAmbient();
                }
                else
                {
                    nightComplete = true;
                }
            }
            else if (nightComplete)
            {
                currentNight = 1;
                gameTime = 0.0;
                fuel = MAX_FUEL;
                ResetGame();
                PlayRandomAmbient();
            }
        }

        if (!finished && !gameOver && !nightComplete)
        {
            gameTime += delta;
            nextLightEvent -= delta;
            nextGpuEvent -= delta;

            if (nextLightEvent <= 0 && mainPower && !lightEventActive)
            {
                mainPower = false;
                lightEventActive = true;
                lightOffTimer = 0.0;
                satoshiTimer = 0.0;
                if (glitchLoaded)
                {
                    StopMusicStream(glitchSound);
                    PlayMusicStream(glitchSound);
                }
                StopAmbient();
            }

            lightOn = mainPower || generatorOn;

            if (!lightOn && !generatorOn && !mainPower)
            {
                satoshiTimer += delta;
                if (satoshiTimer >= 10.0)
                    SetGameOver("SATOSHI ARRIVED! Start the generator!");
            }
            else satoshiTimer = 0.0;

            if (lightEventActive)
            {
                lightOffTimer += delta;
                if (generatorOn) satoshiTimer = 0.0;
                if (lightOffTimer >= lightOffDuration)
                {
                    mainPower = true;
                    lightEventActive = false;
                    lightOffTimer = 0.0;
                    satoshiTimer = 0.0;
                    generatorOn = false;
                    lightOn = true;
                    ScheduleNextLightEvent();
                    if (glitchLoaded) StopMusicStream(glitchSound);
                    ResumeAmbient();
                }
            }

            if (generatorOn && mainPower && !lightEventActive)
            {
                generatorOn = false;
                lightOn = true;
            }

            if (generatorOn)
            {
                fuel -= fuelConsumptionRate * delta;
                if (fuel <= 0.0)
                {
                    fuel = 0.0;
                    generatorOn = false;
                    lightOn = mainPower;
                }
            }

            if (nextGpuEvent <= 0 && !gpuNoise && !gpuEventActive && !doorClosed)
            {
                gpuNoise = true;
                gpuEventActive = true;
                gpuNoiseTimer = 0.0;
                if (fanLoaded)
                {
                    StopMusicStream(fanNoiseSound);
                    PlayMusicStream(fanNoiseSound);
                }
                StopAmbient();
            }
            else if (nextGpuEvent <= 0 && doorClosed)
            {
                ScheduleNextGpuEvent();
            }

            if (doorClosed) gpuTemperature += 1.5 * delta;
            else
            {
                gpuTemperature -= 2.0 * delta;
                if (gpuTemperature < MIN_TEMP) gpuTemperature = MIN_TEMP;
            }
            if (gpuNoise) gpuTemperature += 4.0 * delta;

            if (gpuTemperature >= MAX_TEMP)
            {
                char reason[256];
                std::snprintf(reason, sizeof(reason), "GPU OVERHEATED! %.0f C", gpuTemperature);
                SetGameOver(reason);
            }

            if (gpuEventActive)
            {
                gpuNoiseTimer += delta;
                if (gpuNoiseTimer >= gpuNoiseDuration && gpuTemperature < MAX_TEMP)
                {
                    char reason[256];
                    std::snprintf(reason, sizeof(reason), "YOU BURNED OUT! Had %.1f sec to close door", gpuNoiseDuration);
                    SetGameOver(reason);
                }
            }

            if (gameTime >= GAME_LENGTH)
            {
                gameTime = GAME_LENGTH;
                finished = true;
                generatorOn = false;
                lightOn = true;
                if (fanLoaded) StopMusicStream(fanNoiseSound);
                if (glitchLoaded) StopMusicStream(glitchSound);
                SaveGame(currentNight, fuel);
            }
        }

        if (glitchLoaded && IsMusicStreamPlaying(glitchSound)) UpdateMusicStream(glitchSound);
        if (fanLoaded && IsMusicStreamPlaying(fanNoiseSound)) UpdateMusicStream(fanNoiseSound);
        if (ambientLoaded) UpdateMusicStream(ambientSound);

        char clockText[32];
        FormatGameTime(gameTime, clockText, sizeof(clockText));

        BeginDrawing();
        if (lightOn) ClearBackground(RAYWHITE);
        else ClearBackground(Color{30, 30, 40, 255});

        BeginMode3D(camera);
        DrawModel(scene, Vector3{0, 0, 0}, 1.0f, WHITE);
        EndMode3D();

        Vector2 screenPosition = GetWorldToScreen(clockPosition, camera);
        int fontSize = 60;
        int textWidth = MeasureText(clockText, fontSize);
        int textX = (int)screenPosition.x - textWidth / 2;
        int textY = (int)screenPosition.y - fontSize / 2;
        DrawText(clockText, textX, textY, fontSize, RED);

        DrawText(TextFormat("NIGHT %d / %d", currentNight, maxNights), SCREEN_WIDTH / 2 - 60, 20, 25, YELLOW);

        Color bulbColor = lightOn ? YELLOW : DARKGRAY;
        DrawCircle(SCREEN_WIDTH - 50, 50, 20, bulbColor);
        DrawCircleLines(SCREEN_WIDTH - 50, 50, 20, BLACK);
        DrawText(lightOn ? "LIGHT ON" : "LIGHT OFF", SCREEN_WIDTH - 140, 80, 20, WHITE);

        Color genColor = generatorOn ? GREEN : Color{80, 80, 80, 255};
        DrawRectangle(SCREEN_WIDTH - 150, 110, 130, 30, genColor);
        DrawText(generatorOn ? "GENERATOR: ON" : "GENERATOR: OFF", SCREEN_WIDTH - 145, 117, 18, BLACK);

        DrawRectangle(SCREEN_WIDTH - 150, 150, 130, 20, DARKGRAY);
        Color fuelColor = fuel > 50 ? GREEN : (fuel > 20 ? YELLOW : RED);
        DrawRectangle(SCREEN_WIDTH - 148, 152, (int)((fuel / MAX_FUEL) * 126), 16, fuelColor);
        DrawText(TextFormat("FUEL: %.0f%%", fuel), SCREEN_WIDTH - 140, 153, 16, BLACK);

        Color doorColor = doorClosed ? GREEN : RED;
        DrawRectangle(20, 150, 130, 30, doorColor);
        DrawText(doorClosed ? "DOOR: CLOSED" : "DOOR: OPEN", 25, 157, 18, BLACK);

        if (generatorOn && mainPower && !lightEventActive && (int)(GetTime() * 4) % 2 == 0)
            DrawText("!!! TURN OFF GENERATOR [G] !!!", SCREEN_WIDTH / 2 - 160, 220, 24, YELLOW);

        if (generatorOn && fuel < 20.0 && (int)(GetTime() * 4) % 2 == 0)
            DrawText("!!! LOW FUEL! TURN OFF GENERATOR [G] !!!", SCREEN_WIDTH / 2 - 200, 250, 24, RED);

        if (satoshiTimer > 0.0)
        {
            if ((int)(GetTime() * 5) % 2 == 0)
                DrawText("!!! SATOSHI IS COMING! START GENERATOR [G] !!!", SCREEN_WIDTH / 2 - 280, 280, 26, RED);
            char satoshiText[64];
            std::snprintf(satoshiText, sizeof(satoshiText), "Time left: %.1f sec", 10.0 - satoshiTimer);
            DrawText(satoshiText, SCREEN_WIDTH / 2 - 80, 315, 24, RED);
        }

        if (lightEventActive)
        {
            double timeLeft = lightOffDuration - lightOffTimer;
            if ((int)(GetTime() * 5) % 2 == 0)
                DrawText("!!! LIGHT OFF! TURN RIGHT [D] - START GENERATOR [G] !!!", SCREEN_WIDTH / 2 - 340, 80, 26, RED);
            char timerText[64];
            std::snprintf(timerText, sizeof(timerText), "Power returns in: %.1f sec", timeLeft);
            DrawText(timerText, SCREEN_WIDTH / 2 - 100, 115, 24, timeLeft < 5.0 ? RED : YELLOW);
        }

        if (gpuEventActive)
        {
            double timeLeft = gpuNoiseDuration - gpuNoiseTimer;
            if ((int)(GetTime() * 5) % 2 == 0)
                DrawText("!!! FAN NOISE! TURN LEFT [A] - CLOSE DOOR [F] !!!", SCREEN_WIDTH / 2 - 320, 150, 26, RED);
            char tempText[64];
            std::snprintf(tempText, sizeof(tempText), "GPU: %.0f C / %.0f C | Time: %.1f sec", gpuTemperature, MAX_TEMP, timeLeft);
            DrawText(tempText, SCREEN_WIDTH / 2 - 220, 185, 22, gpuTemperature > 70 ? RED : YELLOW);
        }
        else
        {
            char tempText[64];
            std::snprintf(tempText, sizeof(tempText), "GPU TEMP: %.0f C", gpuTemperature);
            DrawText(tempText, SCREEN_WIDTH / 2 - 100, 80, 20, GREEN);
        }

        const char* yawText = cameraYaw == -1 ? "LEFT (door)" : (cameraYaw == 1 ? "RIGHT (generator)" : "CENTER (clock)");
        DrawText(TextFormat("Camera: %s [A/D]", yawText), 20, 20, 20, GRAY);
        DrawText("F - door (only when looking left)", 20, 50, 18, GRAY);
        DrawText("G - generator (only when looking right)", 20, 75, 18, GRAY);
        DrawText("A/D - rotate camera", 20, 100, 18, GRAY);
        DrawText("SPACE - restart / next night", 20, SCREEN_HEIGHT - 30, 18, GRAY);

        if (IsKeyDown(KEY_F) && cameraYaw != -1)
            DrawText("Turn LEFT to use the door", SCREEN_WIDTH / 2 - 180, SCREEN_HEIGHT - 80, 22, YELLOW);
        if (IsKeyDown(KEY_G) && cameraYaw != 1)
            DrawText("Turn RIGHT to use the generator", SCREEN_WIDTH / 2 - 200, SCREEN_HEIGHT - 80, 22, YELLOW);

        if (!clockExists)
            DrawText("WARNING: Object 'Clock' not found in GLB", 20, 125, 18, ORANGE);

        if (gameOver)
        {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.8f));
            DrawText("GAME OVER", SCREEN_WIDTH / 2 - 180, SCREEN_HEIGHT / 2 - 80, 70, RED);
            DrawText(gameOverReason, SCREEN_WIDTH / 2 - 250, SCREEN_HEIGHT / 2, 28, WHITE);
            DrawText(TextFormat("Night %d failed", currentNight), SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 + 50, 25, GRAY);
            DrawText("Press SPACE to restart", SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 + 90, 25, GRAY);
        }

        if (finished)
        {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.8f));
            DrawText("NIGHT COMPLETE!", SCREEN_WIDTH / 2 - 230, SCREEN_HEIGHT / 2 - 80, 55, GREEN);
            DrawText(TextFormat("Night %d survived!", currentNight), SCREEN_WIDTH / 2 - 130, SCREEN_HEIGHT / 2, 28, WHITE);
            if (currentNight < maxNights)
            {
                DrawText(TextFormat("Next night: %d (Harder!)", currentNight + 1), SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 + 50, 25, YELLOW);
                DrawText("Press SPACE to continue", SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 + 90, 25, GRAY);
            }
            else
            {
                DrawText("ALL NIGHTS SURVIVED!", SCREEN_WIDTH / 2 - 180, SCREEN_HEIGHT / 2 + 50, 35, GOLD);
                DrawText("Press SPACE to finish", SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 + 100, 25, GRAY);
            }
        }

        if (nightComplete)
        {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.9f));
            DrawText("YOU SURVIVED ALL NIGHTS!", SCREEN_WIDTH / 2 - 280, SCREEN_HEIGHT / 2 - 100, 60, GOLD);
            DrawText("The farm ran perfectly!", SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2, 30, WHITE);
            DrawText("Press SPACE to restart", SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 + 70, 25, GRAY);
        }

        EndDrawing();
    }

    if (glitchLoaded) UnloadMusicStream(glitchSound);
    if (fanLoaded) UnloadMusicStream(fanNoiseSound);
    if (ambientLoaded) UnloadMusicStream(ambientSound);
    UnloadModel(scene);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}