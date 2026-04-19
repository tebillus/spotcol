#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILES 256
#define MAX_FILENAME_LENGTH 256

typedef struct {
    char files[MAX_FILES][MAX_FILENAME_LENGTH];
    int fileCount;
    int currentIndex;
    Image originalImage;
    Image workingImage;
    Texture2D texture;
    Color selectedColor;
    float colorTolerance;
    int isModified;
} AppState;

void InitAppState(AppState *state) {
    state->fileCount = 0;
    state->currentIndex = 0;
    state->originalImage.data = NULL;
    state->workingImage.data = NULL;
    state->texture.id = 0;
    state->selectedColor = (Color){255, 100, 100, 255};
    state->colorTolerance = 10.0f;
    state->isModified = 0;
}

int LoadImageFile(AppState *state, const char *filename) {
    if (state->workingImage.data != NULL) {
        UnloadImage(state->workingImage);
    }
    if (state->texture.id != 0) {
        UnloadTexture(state->texture);
    }

    Image img = LoadImage(filename);
    if (img.data == NULL) {
        return 0;
    }

    if (img.mipmaps > 1) {
        ImageMipmaps(&img);
    }

    state->originalImage = img;
    state->workingImage = ImageCopy(img);
    state->texture = LoadTextureFromImage(state->workingImage);
    state->isModified = 0;

    return 1;
}

void ScanDirectory(AppState *state, const char *dirPath, const char *extension) {
    state->fileCount = 0;
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "ls \"%s\"/*.%s 2>/dev/null | sort", dirPath, extension);
    
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) return;
    
    char buffer[MAX_FILENAME_LENGTH];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        buffer[strcspn(buffer, "\n")] = 0;
        if (state->fileCount < MAX_FILES) {
            strncpy(state->files[state->fileCount], buffer, MAX_FILENAME_LENGTH - 1);
            state->fileCount++;
        }
    }
    pclose(fp);
}

void ColorizeImage(AppState *state) {
    if (state->workingImage.data == NULL) return;
    
    Color *pixels = (Color *)state->workingImage.data;
    Color baseColor = state->selectedColor;
    float tolerance = state->colorTolerance;
    
    for (int i = 0; i < state->workingImage.width * state->workingImage.height; i++) {
        if (pixels[i].a > 0) {
            float rDiff = (float)pixels[i].r - (float)baseColor.r;
            float gDiff = (float)pixels[i].g - (float)baseColor.g;
            float bDiff = (float)pixels[i].b - (float)baseColor.b;
            float diff = sqrtf(rDiff*rDiff + gDiff*gDiff + bDiff*bDiff);
            
            if (diff <= tolerance || pixels[i].r == pixels[i].g && pixels[i].g == pixels[i].b) {
                pixels[i].r = baseColor.r;
                pixels[i].g = baseColor.g;
                pixels[i].b = baseColor.b;
            }
            }
        }
        
        if (state->texture.id != 0) {
        UnloadTexture(state->texture);
    }
    state->texture = LoadTextureFromImage(state->workingImage);
    state->isModified = 1;
}

void ResetImage(AppState *state) {
    if (state->originalImage.data == NULL) return;
    
    if (state->workingImage.data != NULL) {
        UnloadImage(state->workingImage);
    }
    state->workingImage = ImageCopy(state->originalImage);
    
    if (state->texture.id != 0) {
        UnloadTexture(state->texture);
    }
    state->texture = LoadTextureFromImage(state->workingImage);
    state->isModified = 0;
}

void FillAlphaRegions(AppState *state) {
    if (state->workingImage.data == NULL) return;
    
    Color *pixels = (Color *)state->workingImage.data;
    Color fillColor = state->selectedColor;
    
    for (int i = 0; i < state->workingImage.width * state->workingImage.height; i++) {
        if (pixels[i].a == 0) {
            pixels[i] = fillColor;
        }
    }
    
    if (state->texture.id != 0) {
        UnloadTexture(state->texture);
    }
    state->texture = LoadTextureFromImage(state->workingImage);
    state->isModified = 1;
}

void ColorSolidPixels(AppState *state) {
    if (state->workingImage.data == NULL) return;
    
    Color *pixels = (Color *)state->workingImage.data;
    Color fillColor = state->selectedColor;
    
    for (int i = 0; i < state->workingImage.width * state->workingImage.height; i++) {
        if (pixels[i].a == 255) {
            pixels[i].r = fillColor.r;
            pixels[i].g = fillColor.g;
            pixels[i].b = fillColor.b;
        }
    }
    
    if (state->texture.id != 0) {
        UnloadTexture(state->texture);
    }
    state->texture = LoadTextureFromImage(state->workingImage);
    state->isModified = 1;
}

void FloodFillImage(AppState *state, int startX, int startY) {
    if (state->workingImage.data == NULL) return;
    if (startX < 0 || startX >= state->workingImage.width || startY < 0 || startY >= state->workingImage.height) return;
    
    Color *pixels = (Color *)state->workingImage.data;
    Color targetColor = pixels[startY * state->workingImage.width + startX];
    Color fillColor = state->selectedColor;
    float tolerance = state->colorTolerance;
    
    int w = state->workingImage.width;
    int h = state->workingImage.height;
    
    typedef struct { int x, y; } Point;
    Point *stack = malloc(w * h * sizeof(Point));
    int stackSize = 0;
    
    stack[stackSize++] = (Point){startX, startY};
    
    while (stackSize > 0) {
        Point p = stack[--stackSize];
        if (p.x < 0 || p.x >= w || p.y < 0 || p.y >= h) continue;
        
        int idx = p.y * w + p.x;
        Color c = pixels[idx];
        
        float rDiff = (float)c.r - (float)targetColor.r;
        float gDiff = (float)c.g - (float)targetColor.g;
        float bDiff = (float)c.b - (float)targetColor.b;
        float aDiff = (float)c.a - (float)targetColor.a;
        float diff = sqrtf(rDiff*rDiff + gDiff*gDiff + bDiff*bDiff + aDiff*aDiff);
        
        if (diff > tolerance) continue;
        
        pixels[idx] = fillColor;
        
        stack[stackSize++] = (Point){p.x + 1, p.y};
        stack[stackSize++] = (Point){p.x - 1, p.y};
        stack[stackSize++] = (Point){p.x, p.y + 1};
        stack[stackSize++] = (Point){p.x, p.y - 1};
    }
    
    free(stack);
    
    if (state->texture.id != 0) {
        UnloadTexture(state->texture);
    }
    state->texture = LoadTextureFromImage(state->workingImage);
    state->isModified = 1;
}

void SaveImage(AppState *state) {
    if (state->workingImage.data == NULL) return;
    
    char outPath[MAX_FILENAME_LENGTH];
    snprintf(outPath, sizeof(outPath), "output_%s", 
             strrchr(state->files[state->currentIndex], '/') + 1);
    
    ExportImage(state->workingImage, outPath);
    state->isModified = 0;
}

void DrawColorPicker(AppState *state, int x, int y) {
    static float hue = 0.0f;
    static float saturation = 0.8f;
    static float value = 1.0f;
    
    Vector3 hsv = ColorToHSV(state->selectedColor);
    hue = hsv.x;
    saturation = hsv.y;
    value = hsv.z;
    
    Rectangle sliderHue = { (float)x, (float)y, 200, 20 };
    Rectangle sliderSat = { (float)x, (float)y + 30, 200, 20 };
    Rectangle sliderVal = { (float)x, (float)y + 60, 200, 20 };
    
    GuiSliderBar(sliderHue, "Hue", NULL, &hue, 0.0f, 360.0f);
    GuiSliderBar(sliderSat, "Sat", NULL, &saturation, 0.0f, 1.0f);
    GuiSliderBar(sliderVal, "Val", NULL, &value, 0.0f, 1.0f);
    
    state->selectedColor = ColorFromHSV(hue, saturation, value);
    
    Rectangle colorPreview = { (float)x + 220, (float)y, 50, 80 };
    DrawRectangleRec(colorPreview, state->selectedColor);
    DrawRectangleLinesEx(colorPreview, 2, BLACK);
}

int main(int argc, char **argv) {
    const int screenWidth = 1200;
    const int screenHeight = 800;
    
    InitWindow(screenWidth, screenHeight, "SpotCol - Image Colorizer");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);
    
    AppState state;
    InitAppState(&state);
    
    const char *imageDir = (argc > 1) ? argv[1] : "png_rgba";
    
    ScanDirectory(&state, "png_rgba", "png");
    ScanDirectory(&state, imageDir, "png");
    
    if (state.fileCount > 0) {
        LoadImageFile(&state, state.files[0]);
    }
    
    while (!WindowShouldClose()) {
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();
        
        if (IsKeyPressed(KEY_RIGHT)) {
            if (state.currentIndex < state.fileCount - 1) {
                state.currentIndex++;
                LoadImageFile(&state, state.files[state.currentIndex]);
            }
        }
        if (IsKeyPressed(KEY_LEFT)) {
            if (state.currentIndex > 0) {
                state.currentIndex--;
                LoadImageFile(&state, state.files[state.currentIndex]);
            }
        }
        
        for (int i = 0; i < 9; i++) {
            if (IsKeyDown(KEY_ONE + i)) {
                state.selectedColor = ColorFromHSV(i * 40.0f, 0.8f, 1.0f);
                break;
            }
        }
        
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && state.workingImage.data != NULL) {
            static double lastClickTime = 0;
            double currentTime = GetTime();
            if (currentTime - lastClickTime > 0.2) {
                lastClickTime = currentTime;
                
                float scale = 1.0f;
                float maxDisplayWidth = screenWidth - 350;
                float maxDisplayHeight = screenHeight - 150;
                
                if (state.workingImage.width > maxDisplayWidth) {
                    scale = maxDisplayWidth / state.workingImage.width;
                }
                if (state.workingImage.height * scale > maxDisplayHeight) {
                    scale = maxDisplayHeight / state.workingImage.height;
                }
                
                int drawWidth = (int)(state.workingImage.width * scale);
                int drawHeight = (int)(state.workingImage.height * scale);
                int drawX = (screenWidth - 350 - drawWidth) / 2;
                int drawY = (screenHeight - 100 - drawHeight) / 2;
                
                Vector2 mouse = GetMousePosition();
                if (mouse.x >= drawX && mouse.x < drawX + drawWidth &&
                    mouse.y >= drawY && mouse.y < drawY + drawHeight) {
                    int imgX = (int)((mouse.x - drawX) / scale);
                    int imgY = (int)((mouse.y - drawY) / scale);
                    FloodFillImage(&state, imgX, imgY);
                }
            }
        }
        
        BeginDrawing();
        ClearBackground((Color){40, 40, 40, 255});
        
        if (state.workingImage.data != NULL) {
            float scale = 1.0f;
            float maxDisplayWidth = screenWidth - 350;
            float maxDisplayHeight = screenHeight - 150;
            
            if (state.workingImage.width > maxDisplayWidth) {
                scale = maxDisplayWidth / state.workingImage.width;
            }
            if (state.workingImage.height * scale > maxDisplayHeight) {
                scale = maxDisplayHeight / state.workingImage.height;
            }
            
            int drawWidth = (int)(state.workingImage.width * scale);
            int drawHeight = (int)(state.workingImage.height * scale);
            int drawX = (screenWidth - 350 - drawWidth) / 2;
            int drawY = (screenHeight - 100 - drawHeight) / 2;
            
            DrawTextureEx(state.texture, (Vector2){(float)drawX, (float)drawY}, 0, scale, WHITE);
        }
        
        int sidebarX = screenWidth - 320;
        
        DrawText("SpotCol", sidebarX, 20, 30, WHITE);
        
        if (state.fileCount > 0) {
            char *filename = strrchr(state.files[state.currentIndex], '/') + 1;
            DrawText(TextFormat("File: %s", filename), sidebarX, 60, 20, LIGHTGRAY);
            DrawText(TextFormat("%d/%d", state.currentIndex + 1, state.fileCount), sidebarX, 85, 15, GRAY);
        }
        
        if (GuiButton((Rectangle){(float)sidebarX, 120, 100, 30}, "Previous")) {
            if (state.currentIndex > 0) {
                state.currentIndex--;
                LoadImageFile(&state, state.files[state.currentIndex]);
            }
        }
        
        if (GuiButton((Rectangle){(float)sidebarX + 110, 120, 100, 30}, "Next")) {
            if (state.currentIndex < state.fileCount - 1) {
                state.currentIndex++;
                LoadImageFile(&state, state.files[state.currentIndex]);
            }
        }
        
        DrawText("Color:", sidebarX, 170, 20, WHITE);
        DrawColorPicker(&state, sidebarX, 200);
        
        static float tolerance = 10.0f;
        Rectangle sliderTol = { (float)sidebarX, 300, 200, 20 };
        GuiSliderBar(sliderTol, "Tolerance", NULL, &tolerance, 1.0f, 100.0f);
        state.colorTolerance = tolerance;
        
        if (GuiButton((Rectangle){(float)sidebarX, 340, 140, 35}, "Fill Alpha")) {
            FillAlphaRegions(&state);
        }
        
        if (GuiButton((Rectangle){(float)sidebarX + 150, 340, 140, 35}, "Color Lines")) {
            ColorSolidPixels(&state);
        }
        
        if (GuiButton((Rectangle){(float)sidebarX, 390, 140, 35}, "Reset")) {
            ResetImage(&state);
        }
        
        if (GuiButton((Rectangle){(float)sidebarX + 150, 390, 140, 35}, "Save Image")) {
            SaveImage(&state);
        }
        
        if (state.isModified) {
            DrawText("* Modified", sidebarX, 440, 20, YELLOW);
        }
        
        DrawText("Controls:", sidebarX, 480, 18, WHITE);
        DrawText("LEFT/RIGHT - Navigate images", sidebarX, 505, 14, GRAY);
        DrawText("1-9 - Quick color presets", sidebarX, 525, 14, GRAY);
        DrawText("Click - Flood fill", sidebarX, 545, 14, GRAY);
        
        EndDrawing();
    }
    
    if (state.workingImage.data != NULL) {
        UnloadImage(state.workingImage);
    }
    if (state.originalImage.data != NULL) {
        UnloadImage(state.originalImage);
    }
    if (state.texture.id != 0) {
        UnloadTexture(state.texture);
    }
    
    CloseWindow();
    
    return 0;
}
