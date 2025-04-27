#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <memory>
#include <fstream>
#include <sstream>

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;
const int COLUMN_COUNT = 4;
const int NOTE_HEIGHT = 20;
const int NOTE_SPEED = 1000; // pixels per second
const int JUDGMENT_LINE_Y = 500;
const int KEY_AREA_HEIGHT = 100;
const SDL_Keycode KEY_BINDINGS[4] = {SDLK_d, SDLK_f, SDLK_j, SDLK_k};

const int AUDIO_FREQUENCY = 44100;
const int AUDIO_CHANNELS = 2;
const int AUDIO_CHUNKSIZE = 4096;

const int PERFECT_WINDOW = 20;
const int GREAT_WINDOW = 50;
const int GOOD_WINDOW = 100;

const int MIN_NOTES_PER_SPAWN = 1;
const int MAX_NOTES_PER_SPAWN = 3;
const float MIN_SPAWN_INTERVAL = 0.3f;
const float MAX_SPAWN_INTERVAL = 0.7f;

enum class JudgmentType {
    PERFECT,
    GREAT,
    GOOD,
    MISS,
    NONE
};

struct BeatmapNote {
    float time;
    int column;
};

struct Note {
    float position;  // Y position
    int column;
    bool hit;
    bool missed;
    SDL_Rect rect;
    SDL_Color color;
};

struct Judgment {
    JudgmentType type;
    float displayTime;
    SDL_Color color;
    std::string text;
};

class Beatmap {
    private:
        std::vector<BeatmapNote> notes;
        bool loaded;
        std::string title;
        std::string musicFile;
        float offset;
        float songLength;
        
    public:
        Beatmap() : loaded(false), songLength(0.0f), offset(0.0f) {}
        
        bool loadFromFile(const std::string& filename) {
            std::ifstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Failed to open beatmap file: " << filename << std::endl;
                return false;
            }
            
            notes.clear();
            std::string line;
            
            if (std::getline(file, line)) {
                title = line;
            }
            
            if (std::getline(file, line)) {
                musicFile = line;
            }
            
            if (std::getline(file, line)) {
                try {
                    offset = std::stof(line) / 1000.0f;
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing offset: " << line << " - " << e.what() << std::endl;
                    offset = 0.0f;
                }
            }
            
            while (std::getline(file, line)) {
                std::istringstream iss(line);
                std::string timeStr, columnStr;
                
                if (line.empty() || line[0] == '#' || line[0] == '/') {
                    continue;
                }
                
                if (std::getline(iss, timeStr, ',') && std::getline(iss, columnStr)) {
                    try {
                        float time = std::stof(timeStr);
                        int column = std::stoi(columnStr);
                        
                        if (column >= 0 && column < COLUMN_COUNT) {
                            BeatmapNote note;
                            note.time = time;
                            note.column = column;
                            notes.push_back(note);
                            
                            if (time > songLength) {
                                songLength = time;
                            }
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error parsing line: " << line << " - " << e.what() << std::endl;
                    }
                }
            }
            
            songLength += 5.0f;
            
            std::sort(notes.begin(), notes.end(), 
                  [](const BeatmapNote& a, const BeatmapNote& b) {
                      return a.time < b.time;
                  });
            
            loaded = !notes.empty() && !musicFile.empty();
            return loaded;
        }
        
        bool isLoaded() const { return loaded; }
        const std::string& getTitle() const { return title; }
        const std::string& getMusicFile() const { return musicFile; }
        float getOffset() const { return offset; }
        float getSongLength() const { return songLength; }
        
        std::vector<int> getNotesAtTime(float currentTime) {
            std::vector<int> columns;
            
            for (auto it = notes.begin(); it != notes.end(); ) {
                if (std::abs(it->time - currentTime) < 0.01f) {
                    columns.push_back(it->column);
                    it = notes.erase(it);
                } else if (it->time > currentTime) {
                    break;
                } else {
                    ++it;
                }
            }
            
            return columns;
        }
        
        bool hasMoreNotes() const {
            return !notes.empty();
        }
    };

class OsuMania {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;

    Mix_Music* music;
    bool musicPlaying;
    float musicStartTime;
    bool musicLoaded;
    
    std::vector<Note> notes;
    bool keyStates[COLUMN_COUNT];
    bool gameRunning;
    bool gameStarted;
    bool gameEnded;
    
    int score;
    int combo;
    int maxCombo;
    int totalHits;
    int perfectHits;
    int greatHits;
    int goodHits;
    int missedHits;
    
    float columnWidth;
    Judgment currentJudgment;
    
    std::mt19937 rng;

    std::chrono::time_point<std::chrono::high_resolution_clock> lastFrameTime;
    std::chrono::time_point<std::chrono::high_resolution_clock> gameStartTime;
    float noteGenerationTimer;
    float nextGenerationInterval;
    
    Beatmap currentBeatmap;
    float gameTime;
    bool useRandomNotes;
    std::string beatmapFile;
    
    public:
    OsuMania() : 
        window(nullptr), 
        renderer(nullptr), 
        font(nullptr),
        music(nullptr),
        musicPlaying(false),
        musicStartTime(0.0f),
        musicLoaded(false),
        gameRunning(true),
        gameStarted(false),
        gameEnded(false),
        score(0),
        combo(0),
        maxCombo(0),
        totalHits(0),
        perfectHits(0),
        greatHits(0),
        goodHits(0),
        missedHits(0),
        columnWidth(SCREEN_WIDTH / COLUMN_COUNT),
        noteGenerationTimer(0.0f),
        nextGenerationInterval(0.5f),
        gameTime(0.0f),
        useRandomNotes(true),
        beatmapFile("his_theme.txt")
    {
        for (int i = 0; i < COLUMN_COUNT; i++) {
            keyStates[i] = false;
        }
        
        currentJudgment.type = JudgmentType::NONE;
        currentJudgment.displayTime = 0.0f;
        
        std::random_device rd;
        rng = std::mt19937(rd());
    }
    
    ~OsuMania() {
        std::cout << "Destroying OsuMania object" << std::endl;
        cleanup();
    }
    
    bool initialize(const std::string& mapFile = "") {
        if (!mapFile.empty()) {
            beatmapFile = mapFile;
        }
        
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
            std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }
        
        if (TTF_Init() < 0) {
            std::cerr << "SDL_ttf could not initialize! TTF_Error: " << TTF_GetError() << std::endl;
            return false;
        }
        
        if (Mix_OpenAudio(AUDIO_FREQUENCY, MIX_DEFAULT_FORMAT, AUDIO_CHANNELS, AUDIO_CHUNKSIZE) < 0) {
            std::cerr << "SDL_mixer could not initialize! Mix_Error: " << Mix_GetError() << std::endl;
            return false;
        }
        
        window = SDL_CreateWindow("osu!mania Clone", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                                 SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
        if (window == nullptr) {
            std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }
        
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (renderer == nullptr) {
            std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }
        
        font = TTF_OpenFont("fonts/arial.ttf", 24);
        if (font == nullptr) {
            std::cerr << "Failed to load font! TTF_Error: " << TTF_GetError() << std::endl;

            font = TTF_OpenFont("fonts/FreeSans.ttf", 24);
            if (font == nullptr) {
                std::cerr << "Failed to load fallback font! TTF_Error: " << TTF_GetError() << std::endl;
                return false;
            }
        }
        
        if (currentBeatmap.loadFromFile(beatmapFile)) {
            useRandomNotes = false;
            std::cout << "Loaded beatmap: " << currentBeatmap.getTitle() << std::endl;
            std::cout << "Music file: " << currentBeatmap.getMusicFile() << std::endl;
            
            loadMusic(currentBeatmap.getMusicFile());
        } else {
            useRandomNotes = true;
            std::cout << "Using random note generation (beatmap file not found or invalid)" << std::endl;
        }
        
        lastFrameTime = std::chrono::high_resolution_clock::now();
        
        return true;
    }

    bool loadMusic(const std::string& musicPath) {
        if (music != nullptr) {
            Mix_FreeMusic(music);
            music = nullptr;
        }
        
        // Load the music file
        music = Mix_LoadMUS(musicPath.c_str());
        if (music == nullptr) {
            std::cerr << "Failed to load music! Mix_Error: " << Mix_GetError() << std::endl;
            musicLoaded = false;
            return false;
        }
        
        musicLoaded = true;
        std::cout << "Music loaded successfully: " << musicPath << std::endl;
        return true;
    }
    
    void cleanup() {
        std::cout << "Performing cleanup..." << std::endl;
    
        notes.clear();
    
        if (music != nullptr) {
            Mix_HaltMusic();
            Mix_FreeMusic(music);
            music = nullptr;
        }
        
        if (font != nullptr) {
            TTF_CloseFont(font);
            font = nullptr;
        }
        
        if (renderer != nullptr) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        
        if (window != nullptr) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }
        
        Mix_CloseAudio();
        TTF_Quit();
        SDL_Quit();
    
        std::cout << "Cleanup complete" << std::endl;
    }

    void shutdown() {
        gameRunning = false;
    }
    
    void run() {
    
        SDL_Event e;
        
        while (gameRunning) {
            while (SDL_PollEvent(&e)) {
                handleEvent(e);
            }

            if (!gameRunning) {
                std::cout << "Exiting game loop" << std::endl;
                break;
            }
            
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
            lastFrameTime = currentTime;
            
            if (gameStarted) {
                update(deltaTime);
            }
            
            render();
            
            SDL_Delay(16);   //this is for the game to run at 60 FPS

            static int frameCount = 0;
            frameCount++;
            if (frameCount % 300 == 0) {
                std::cout << "Frame: " << frameCount << std::endl;
            }
        }

        std::cout << "Game loop ended, cleaning up" << std::endl;
    }
    
    void handleEvent(SDL_Event& e) {
        if (e.type == SDL_QUIT) {
            shutdown();
        }
        else if (e.type == SDL_KEYDOWN) {

            if (e.key.keysym.sym == SDLK_ESCAPE) {
                shutdown();
            }

            else if (gameEnded) {
                if (e.key.keysym.sym == SDLK_SPACE) {
                    gameEnded = false; 
                    resetStats();      
                    startGame();       
                }
                return;
            }

            else if (e.key.keysym.sym == SDLK_r && !gameEnded) {
                stopMusic();
                resetStats();
                gameStarted = false;
                if (currentBeatmap.loadFromFile(beatmapFile)) {
                    useRandomNotes = false;
                    std::cout << "Reloaded beatmap: " << currentBeatmap.getTitle() << std::endl;
                } else {
                    useRandomNotes = true;
                    std::cout << "Using random note generation (beatmap reload failed)" << std::endl;
                }
            }
            else if (e.key.keysym.sym == SDLK_SPACE && !gameStarted && !gameEnded) {
                startGame();
            }
            
            if (gameStarted && !gameEnded) {
                for (int i = 0; i < COLUMN_COUNT; i++) {
                    if (e.key.keysym.sym == KEY_BINDINGS[i] && !keyStates[i]) {
                        keyStates[i] = true;
                        handleKeyPress(i);
                    }
                }
            }
        } else if (e.type == SDL_KEYUP) {
            for (int i = 0; i < COLUMN_COUNT; i++) {
                if (e.key.keysym.sym == KEY_BINDINGS[i]) {
                    keyStates[i] = false;
                }
            }
        }
    }
    
    void startGame() {
        gameStarted = true;
        resetStats();
        gameStartTime = std::chrono::high_resolution_clock::now();
        gameTime = 0.0f;

        if (musicLoaded && !useRandomNotes) {
            playMusic();
            musicStartTime = 0.0f;
        }
    }

    void playMusic() {
        if (music != nullptr) {
            Mix_PlayMusic(music, 0);
            musicPlaying = true;
            std::cout << "Music playback started" << std::endl;
        }
    }
    
    void stopMusic() {
        if (musicPlaying) {
            Mix_HaltMusic();
            musicPlaying = false;
        }
    }
    
    void resetStats() {
        score = 0;
        combo = 0;
        maxCombo = 0;
        totalHits = 0;
        perfectHits = 0;
        greatHits = 0;
        goodHits = 0;
        missedHits = 0;
        notes.clear();
        gameTime = 0.0f;
        gameEnded = false;
        
        std::uniform_real_distribution<float> timeDist(MIN_SPAWN_INTERVAL, MAX_SPAWN_INTERVAL);
        nextGenerationInterval = timeDist(rng);
        noteGenerationTimer = 0.0f;
    }
    
    void update(float deltaTime) {
        gameTime += deltaTime;

        if (musicPlaying && !Mix_PlayingMusic()) {
            musicPlaying = false;
            std::cout << "Music playback ended" << std::endl;
        }
        
        if (!useRandomNotes) {
            float adjustedTime = gameTime - currentBeatmap.getOffset();
            
            std::vector<int> noteColumns = currentBeatmap.getNotesAtTime(adjustedTime);
            for (int column : noteColumns) {
                createNote(column);
            }
            
            if (!currentBeatmap.hasMoreNotes() && notes.empty() && 
                gameTime > (currentBeatmap.getSongLength() + currentBeatmap.getOffset()) && 
                !musicPlaying) {
                showResults();
                gameStarted = false;
                gameEnded = true;
            }
        } else {
            noteGenerationTimer += deltaTime;
            if (noteGenerationTimer > nextGenerationInterval) {

                std::uniform_int_distribution<int> notesCountDist(MIN_NOTES_PER_SPAWN, MAX_NOTES_PER_SPAWN);
                int notesToGenerate = notesCountDist(rng);
                
                generateNotePattern(notesToGenerate);
                
                noteGenerationTimer = 0.0f;
                std::uniform_real_distribution<float> timeDist(MIN_SPAWN_INTERVAL, MAX_SPAWN_INTERVAL);
                nextGenerationInterval = timeDist(rng);
            }
        }
        
        for (auto& note : notes) {
            if (!note.hit && !note.missed) {
                note.position += NOTE_SPEED * deltaTime;
                note.rect.y = static_cast<int>(note.position);
                
                if (note.position > JUDGMENT_LINE_Y + NOTE_HEIGHT * 2) {
                    note.missed = true;
                    handleMiss();
                }
            }
        }
        
        if (currentJudgment.type != JudgmentType::NONE) {
            currentJudgment.displayTime -= deltaTime;
            if (currentJudgment.displayTime <= 0.0f) {
                currentJudgment.type = JudgmentType::NONE;
            }
        }
        
        notes.erase(
            std::remove_if(notes.begin(), notes.end(), 
                [](const Note& note) { return note.hit || note.missed; }),
            notes.end()
        );
    }
    
    void generateNotePattern(int notesCount) {
        switch(notesCount) {
            case 1:
                {
                    std::uniform_int_distribution<int> columnDist(0, COLUMN_COUNT - 1);
                    int columnIndex = columnDist(rng);
                    createNote(columnIndex);
                }
                break;
            
            case 2:
                {
                    std::uniform_int_distribution<int> patternDist(0, 1);
                    int patternType = patternDist(rng);
                    
                    if (patternType == 0) {
                        std::uniform_int_distribution<int> startColDist(0, COLUMN_COUNT - 2);
                        int startCol = startColDist(rng);
                        createNote(startCol);
                        createNote(startCol + 1);
                    } else {
                        createNote(0);
                        createNote(COLUMN_COUNT - 1);
                    }
                }
                break;
            
            case 3:
                {
                    std::vector<int> availableCols;
                    for (int i = 0; i < COLUMN_COUNT; i++) {
                        availableCols.push_back(i);
                    }
                    
                    std::shuffle(availableCols.begin(), availableCols.end(), rng);
                    
                    for (int i = 0; i < 3 && i < COLUMN_COUNT; i++) {
                        createNote(availableCols[i]);
                    }
                }
                break;
                
            default:
                {
                    std::uniform_int_distribution<int> columnDist(0, COLUMN_COUNT - 1);
                    int columnIndex = columnDist(rng);
                    createNote(columnIndex);
                }
                break;
        }
    }
    
    void createNote(int columnIndex) {
        Note note;
        note.position = 0;
        note.column = columnIndex;
        note.hit = false;
        note.missed = false;
        
        int noteWidth = static_cast<int>(columnWidth) - 10;
        note.rect.x = static_cast<int>(columnIndex * columnWidth) + 5;
        note.rect.y = 0;
        note.rect.w = noteWidth;
        note.rect.h = NOTE_HEIGHT;
        
        switch (columnIndex) {
            case 0: note.color = {255, 100, 100, 255}; break; // red
            case 1: note.color = {100, 255, 100, 255}; break; // green
            case 2: note.color = {100, 100, 255, 255}; break; // blue
            case 3: note.color = {255, 255, 100, 255}; break; // yellow
            default: note.color = {255, 255, 255, 255}; break;
        }
        
        notes.push_back(note);
    }
    
    void handleKeyPress(int columnIndex) {
        if (!gameStarted) return;
        
        Note* closestNote = nullptr;
        float closestDistance = std::numeric_limits<float>::max();
        
        for (auto& note : notes) {
            if (note.column == columnIndex && !note.hit && !note.missed) {
                float distance = std::abs(note.position - JUDGMENT_LINE_Y);
                if (distance < closestDistance) {
                    closestDistance = distance;
                    closestNote = &note;
                }
            }
        }
        
        if (closestNote != nullptr && closestDistance < GOOD_WINDOW) {
            closestNote->hit = true;
            totalHits++;
            
            if (closestDistance < PERFECT_WINDOW) {
                showJudgment(JudgmentType::PERFECT);
                score += 300 + combo * 5;
                combo++;
                perfectHits++;
            } else if (closestDistance < GREAT_WINDOW) {
                showJudgment(JudgmentType::GREAT);
                score += 200 + combo * 3;
                combo++;
                greatHits++;
            } else {
                showJudgment(JudgmentType::GOOD);
                score += 100 + combo;
                combo++;
                goodHits++;
            }
            
            if (combo > maxCombo) {
                maxCombo = combo;
            }
        }
    }
    
    void handleMiss() {
        showJudgment(JudgmentType::MISS);
        combo = 0;
        totalHits++;
        missedHits++;
    }
    
    void showJudgment(JudgmentType type) {
        currentJudgment.type = type;
        currentJudgment.displayTime = 0.5f;
        
        switch (type) {
            case JudgmentType::PERFECT:
                currentJudgment.text = "PERFECT";
                currentJudgment.color = {255, 230, 0, 255}; // gold
                break;
            case JudgmentType::GREAT:
                currentJudgment.text = "GREAT";
                currentJudgment.color = {0, 255, 0, 255}; // green
                break;
            case JudgmentType::GOOD:
                currentJudgment.text = "GOOD";
                currentJudgment.color = {0, 200, 255, 255}; // blue
                break;
            case JudgmentType::MISS:
                currentJudgment.text = "MISS";
                currentJudgment.color = {255, 0, 0, 255}; // red
                break;
            default:
                break;
        }
    }
    
    void showResults() {
        std::cout << "\n===== RESULTS =====\n";
        std::cout << "Score: " << score << std::endl;
        std::cout << "Max Combo: " << maxCombo << "x" << std::endl;
        
        float accuracy = 100.0f;
        if (totalHits > 0) {
            accuracy = (perfectHits * 300.0f + greatHits * 200.0f + goodHits * 100.0f) / 
                      (totalHits * 300.0f) * 100.0f;
        }
        
        std::cout << "Accuracy: " << accuracy << "%" << std::endl;
        std::cout << "Perfect: " << perfectHits << std::endl;
        std::cout << "Great: " << greatHits << std::endl;
        std::cout << "Good: " << goodHits << std::endl;
        std::cout << "Miss: " << missedHits << std::endl;
        std::cout << "==================\n";
    }
    
    void render() {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        for (int i = 0; i < COLUMN_COUNT; i++) {
            // Column borders
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            SDL_Rect columnRect = {
                static_cast<int>(i * columnWidth),
                0,
                static_cast<int>(columnWidth),
                SCREEN_HEIGHT
            };
            SDL_RenderDrawRect(renderer, &columnRect);
            
            SDL_Rect keyRect = {
                static_cast<int>(i * columnWidth),
                JUDGMENT_LINE_Y,
                static_cast<int>(columnWidth),
                KEY_AREA_HEIGHT
            };
            
            if (keyStates[i]) {
                SDL_SetRenderDrawColor(renderer, 66, 135, 245, 200);
            } else {
                SDL_SetRenderDrawColor(renderer, 30, 30, 30, 200);
            }
            SDL_RenderFillRect(renderer, &keyRect);
            
            std::string keyText;
            switch (KEY_BINDINGS[i]) {
                case SDLK_d: keyText = "D"; break;
                case SDLK_f: keyText = "F"; break;
                case SDLK_j: keyText = "J"; break;
                case SDLK_k: keyText = "K"; break;
                default: keyText = "?"; break;
            }
            renderText(keyText, 
                      static_cast<int>(i * columnWidth + columnWidth / 2 - 5), 
                      JUDGMENT_LINE_Y + KEY_AREA_HEIGHT / 2 - 10,
                      {200, 200, 200, 255});
        }
        
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_Rect lineRect = {0, JUDGMENT_LINE_Y, SCREEN_WIDTH, 3};
        SDL_RenderFillRect(renderer, &lineRect);
        
        for (const auto& note : notes) {
            if (!note.hit && !note.missed) {
                SDL_SetRenderDrawColor(renderer, note.color.r, note.color.g, note.color.b, note.color.a);
                SDL_RenderFillRect(renderer, &note.rect);
            }
        }
        
        renderText("Score: " + std::to_string(score), 10, 10, {255, 255, 255, 255});
        renderText("Combo: " + std::to_string(combo) + "x", 10, 40, {255, 255, 255, 255});
        
        float accuracy = 100.0f;
        if (totalHits > 0) {
            accuracy = (perfectHits * 300.0f + greatHits * 200.0f + goodHits * 100.0f) / 
                      (totalHits * 300.0f) * 100.0f;
        }
        
        renderText("Acc: " + std::to_string(static_cast<int>(accuracy)) + "." + 
                   std::to_string(static_cast<int>(accuracy * 100) % 100) + "%", 
                   10, 70, {255, 255, 255, 255});
        
        if (!useRandomNotes) {
            renderText("Time: " + std::to_string(static_cast<int>(gameTime)), 10, 100, {255, 255, 255, 255});
        }
        
        if (currentJudgment.type != JudgmentType::NONE) {
            renderText(currentJudgment.text, 
                      SCREEN_WIDTH / 2 - 50, 
                      JUDGMENT_LINE_Y - 50,
                      currentJudgment.color);
        }
        
        if (!useRandomNotes) {
            renderText(currentBeatmap.getTitle(), 
                      SCREEN_WIDTH / 2 - 100, 
                      10,
                      {200, 200, 255, 255});
        } else {
            renderText("Random Mode", 
                      SCREEN_WIDTH / 2 - 50, 
                      10,
                      {200, 200, 255, 255});
        }

        if (gameEnded) {
            renderText("Ending", 
                      SCREEN_WIDTH / 2 - 60, 
                      SCREEN_HEIGHT / 4,
                      {255, 100, 100, 255});
                      
            renderText("Final Score: " + std::to_string(score), 
                      SCREEN_WIDTH / 2 - 100, 
                      SCREEN_HEIGHT / 2 - 60,
                      {255, 255, 255, 255});
                      
            renderText("Max Combo: " + std::to_string(maxCombo) + "x", 
                      SCREEN_WIDTH / 2 - 100, 
                      SCREEN_HEIGHT / 2 - 30,
                      {255, 255, 255, 255});
                      
            float accuracy = 100.0f;
            if (totalHits > 0) {
                accuracy = (perfectHits * 300.0f + greatHits * 200.0f + goodHits * 100.0f) / 
                         (totalHits * 300.0f) * 100.0f;
            }
            
            renderText("Accuracy: " + std::to_string(static_cast<int>(accuracy)) + "." + 
                     std::to_string(static_cast<int>(accuracy * 100) % 100) + "%", 
                     SCREEN_WIDTH / 2 - 100, 
                     SCREEN_HEIGHT / 2,
                     {255, 255, 255, 255});
                     
            renderText("Perfect: " + std::to_string(perfectHits), 
                     SCREEN_WIDTH / 2 - 100, 
                     SCREEN_HEIGHT / 2 + 30,
                     {255, 230, 0, 255});
                     
            renderText("Great: " + std::to_string(greatHits), 
                     SCREEN_WIDTH / 2 - 100, 
                     SCREEN_HEIGHT / 2 + 60,
                     {0, 255, 0, 255});
                     
            renderText("Good: " + std::to_string(goodHits), 
                     SCREEN_WIDTH / 2 - 100, 
                     SCREEN_HEIGHT / 2 + 90,
                     {0, 200, 255, 255});
                     
            renderText("Miss: " + std::to_string(missedHits), 
                     SCREEN_WIDTH / 2 - 100, 
                     SCREEN_HEIGHT / 2 + 120,
                     {255, 0, 0, 255});
                     
            renderText("Press SPACE to restart", 
                     SCREEN_WIDTH / 2 - 120, 
                     SCREEN_HEIGHT - 60,
                     {255, 255, 255, 255});
                     
            SDL_RenderPresent(renderer);
            return;
        }
        
        if (!gameStarted) {

            renderText("Press SPACE to start", 
                      SCREEN_WIDTH / 2 - 100, 
                      SCREEN_HEIGHT / 2,
                      {255, 255, 255, 255});
                      
            renderText("Press R to reload beatmap", 
                      SCREEN_WIDTH / 2 - 120, 
                      SCREEN_HEIGHT / 2 + 30,
                      {200, 200, 200, 255});
        }
        
        SDL_RenderPresent(renderer);
    }
    
    void renderText(const std::string& text, int x, int y, SDL_Color color) {
        if (font == nullptr) return;
        
        SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
        if (surface == nullptr) {
            std::cerr << "Unable to render text surface! TTF_Error: " << TTF_GetError() << std::endl;
            return;
        }
        
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture == nullptr) {
            std::cerr << "Unable to create texture from rendered text! SDL_Error: " << SDL_GetError() << std::endl;
            SDL_FreeSurface(surface);
            return;
        }
        
        SDL_Rect renderRect = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &renderRect);
        
        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);
    }
};

int main(int argc, char* argv[]) {
    SDL_SetMainReady();

    std::string beatmapFile = "his_theme.txt";
    
    if (argc > 1) {
        beatmapFile = argv[1];
    }
    
    {
        std::cout << "Creating game instance..." << std::endl;
        OsuMania game;
        
        if (!game.initialize(beatmapFile)) {
            std::cerr << "Failed to initialize game" << std::endl;
            return 1;
        }
        
        game.run();
    }
    
    std::cout << "Program exiting normally" << std::endl;
    return 0;
}
