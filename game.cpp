#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <array>
#include <format>
#include <iomanip>
#include <conio.h>
#include <windows.h>
#include <numeric>

// ============================================================================
// Global Constants & Types
// ============================================================================
constexpr int PIECE_COUNT = 6;
constexpr int CODE_LENGTH = 4;
constexpr int MAX_TRIES   = 10;

// Strongly typed enumeration
enum class GameMode { EASY = 0, MEDIUM = 1, HARD = 2 };

// Record type for piece definition
struct PieceDefinition {
    WORD        consoleColor;
    char        label;
    std::string name;
};

// Static lookup table
const std::array<PieceDefinition, PIECE_COUNT> TILES = {{
    { FOREGROUND_BLUE  | FOREGROUND_INTENSITY,                                 'B', "BLUE " },
    { FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,               'C', "CYAN " },
    { FOREGROUND_RED   | FOREGROUND_BLUE | FOREGROUND_INTENSITY,               'M', "MAGENTA " },
    { FOREGROUND_BLUE  | FOREGROUND_GREEN,                                     'L', "LIGHT BLUE " },
    { FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY, 'W', "WHITE " },
    { FOREGROUND_RED   | FOREGROUND_GREEN | FOREGROUND_INTENSITY,              'Y', "YELLOW " }
}};

// Console colors
const WORD FB_GREEN  = FOREGROUND_GREEN  | FOREGROUND_INTENSITY;
const WORD FB_YELLOW = FOREGROUND_RED    | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
const WORD FB_RED    = FOREGROUND_RED    | FOREGROUND_INTENSITY;
const WORD FB_EMPTY  = FOREGROUND_RED    | FOREGROUND_GREEN | FOREGROUND_BLUE;

// Global associative array for player stats
std::map<std::string, int> g_playerLifetimeWins;

// Global counter for side-effect demo
int g_sideEffectCounter = 0;

// ============================================================================
// Game State Struct with Operator Overloading
// ============================================================================
struct GameState {
    std::array<int, CODE_LENGTH> secretCode;
    std::array<int, CODE_LENGTH> currentGuess;
    GameMode                     mode;
    int                          attemptNumber;
    int                          totalAttempts;
    bool                         playerTwoWon;
    int                          lastCorrectPlace;
    int                          lastCorrectColor;
    double                       accuracyPercent;

    GameState& operator+=(int attempts) {
        this->totalAttempts += attempts;
        return *this;
    }

    bool operator==(const std::array<int, CODE_LENGTH>& rhsTarget) const {
        for (size_t i = 0; i < CODE_LENGTH; ++i)
            if (this->secretCode[i] != rhsTarget[i]) return false;
        return true;
    }
};

// ============================================================================
// Function Prototypes
// ============================================================================
void setCursorPosition(int col, int row);
void setTextColor(WORD colorAttr);
void clearScreen();
void hideCursor();
void showCursor();
void drawTitleBar();
void drawSidePanels(GameMode mode, bool isPlayer1Phase);
void drawGrid();
void renderGuessRow(const std::array<int, CODE_LENGTH>& code, int activeTile, int row, GameMode mode, bool revealColors);
void cleanRowCursor(int row);
void drawFeedbackOnRow(const std::array<int, CODE_LENGTH>& guess, const std::array<int, CODE_LENGTH>& tileFeedback, int row, GameMode mode);
void showInlineCounts(int correctPlace, int correctColor, int row, GameMode mode, bool showDiagnostics = false);
void showInlineCounts(int correctPlace, int correctColor, int row, GameMode mode, const std::string& devTelemetry);
GameMode selectDifficulty();
void player1EnterCode(GameState& state, std::array<int, CODE_LENGTH>& outCode);
void player2GuessLoop(GameState& state, std::vector<std::vector<int>>& sessionHistory);
void displayEndScreen(const GameState& state, const std::string& p2Name);
double calculateAccuracy(int correctPlace, int totalAttempts);
void calculateFeedback(const std::array<int, CODE_LENGTH>& secret, const std::array<int, CODE_LENGTH>& guess,
                      int& outCorrectPlace, int& outCorrectColor, std::array<int, CODE_LENGTH>& outTileFeedback);
template <typename T, size_t Size> void resetArrayProfile(std::array<T, Size>& targetArray, T fillVal);

// ============================================================================
// Console Manipulation
// ============================================================================
void setCursorPosition(int col, int row) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD pos = { static_cast<SHORT>(col), static_cast<SHORT>(row) };
    SetConsoleCursorPosition(hConsole, pos);
}

void setTextColor(WORD colorAttr) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, colorAttr);
}

void clearScreen() { system("cls"); }
void hideCursor() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = false;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}
void showCursor() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = true;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}

// ============================================================================
// Game Logic Helpers
// ============================================================================
double calculateAccuracy(int correctPlace, int totalAttempts) {
    if (totalAttempts <= 0) return 0.0;
    double rawRatio = static_cast<double>(correctPlace) / static_cast<double>(totalAttempts * CODE_LENGTH);
    double percentage = rawRatio * 100.0;
    // Narrowing conversion demo (explicit cast)
    int truncated = static_cast<int>(percentage);
    (void)truncated;
    return percentage;
}

void calculateFeedback(const std::array<int, CODE_LENGTH>& secret,
                       const std::array<int, CODE_LENGTH>& guess,
                       int& outCorrectPlace, int& outCorrectColor,
                       std::array<int, CODE_LENGTH>& outTileFeedback) {
    outCorrectPlace = 0;
    outCorrectColor = 0;
    std::array<bool, CODE_LENGTH> secretUsed = {false};
    std::array<bool, CODE_LENGTH> guessUsed  = {false};
    outTileFeedback.fill(3);

    for (int i = 0; i < CODE_LENGTH; ++i) {
        if (secret[i] == guess[i]) {
            outCorrectPlace++;
            secretUsed[i] = guessUsed[i] = true;
            outTileFeedback[i] = 1;
        }
    }
    for (int g = 0; g < CODE_LENGTH; ++g) {
        if (!guessUsed[g]) {
            for (int s = 0; s < CODE_LENGTH; ++s) {
                // Short-circuit evaluation demo
                if (!secretUsed[s] && guess[g] == secret[s]) {
                    outCorrectColor++;
                    secretUsed[s] = true;
                    outTileFeedback[g] = 2;
                    break;
                }
            }
        }
    }
    outCorrectColor += outCorrectPlace;
}

template <typename T, size_t Size>
void resetArrayProfile(std::array<T, Size>& targetArray, T fillVal) {
    targetArray.fill(fillVal);
}

// ============================================================================
// UI Drawing Functions
// ============================================================================
void drawTitleBar() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    setCursorPosition(40, 1);
    SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    std::cout << "----------------------------------------";
    setCursorPosition(47, 2);
    std::cout << " COLOR CODE GUESSING GAME ";
    setCursorPosition(40, 3);
    std::cout << "----------------------------------------";
    setTextColor(FB_EMPTY);
}

void drawSidePanels(GameMode mode, bool isPlayer1Phase) {
    setTextColor(FB_EMPTY);
    // Left panel (controls)
    setCursorPosition(2, 5);  std::cout << "-----------------------";
    setCursorPosition(2, 6);  std::cout << "  L/R Arrow : Tile     ";
    setCursorPosition(2, 7);  std::cout << "  U/D Arrow : Color    ";
    setCursorPosition(2, 8);  std::cout << "  ENTER     : Submit   ";
    setCursorPosition(2, 9);  std::cout << "-----------------------";

    // Mode display
    setCursorPosition(2, 11);
    switch (mode) {
        case GameMode::EASY:   setTextColor(FB_GREEN);  std::cout << "MODE: EASY  "; break;
        case GameMode::MEDIUM: setTextColor(FB_YELLOW); std::cout << "MODE: MEDIUM"; break;
        case GameMode::HARD:   setTextColor(FB_RED);    std::cout << "MODE: HARD  "; break;
    }
    setTextColor(FB_EMPTY);

    // Right panel (feedback legend)
    if (mode != GameMode::HARD && !isPlayer1Phase) {
        setCursorPosition(95, 5); setTextColor(FB_GREEN);  std::cout << "GREEN = Correct";
        if (mode == GameMode::EASY) {
            setCursorPosition(95, 6); setTextColor(FB_YELLOW); std::cout << "YELLOW = Wrong Position";
        }
        setCursorPosition(95, 7); setTextColor(FB_RED);    std::cout << "RED   = Not Found";
        setTextColor(FB_EMPTY);
    }

    // Tile legend (only during player 1 setup)
    if (isPlayer1Phase) {
        setCursorPosition(90, 10); setTextColor(FB_EMPTY); std::cout << "AVAILABLE TILES:";
        for (int i = 0; i < PIECE_COUNT; ++i) {
            setCursorPosition(90, 12 + i);
            setTextColor(TILES[i].consoleColor);
            std::cout << "  " << TILES[i].name[0] << " = " << TILES[i].name;
        }
        setTextColor(FB_EMPTY);
    }
}

void drawGrid() {
    const int GRID_START_ROW = 6;
    const int GRID_START_COL = 44;
    const int BOX_STEP = 9;
    for (int row = 0; row < MAX_TRIES; ++row) {
        int displayRow = GRID_START_ROW + row * 2;
        for (int col = 0; col < CODE_LENGTH; ++col) {
            int displayCol = GRID_START_COL + col * BOX_STEP;
            setCursorPosition(displayCol, displayRow);
            std::cout << "[   ]";
        }
    }
}

void renderGuessRow(const std::array<int, CODE_LENGTH>& code, int activeTile, int row, GameMode mode, bool revealColors) {
    const int GRID_START_COL = 44;
    const int BOX_STEP = 9;
    for (int i = 0; i < CODE_LENGTH; ++i) {
        int displayCol = GRID_START_COL + i * BOX_STEP;
        setCursorPosition(displayCol, row);
        if (i == activeTile) {
            setTextColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            std::cout << ">";
        } else {
            std::cout << " ";
        }
        int pieceIdx = code[i];
        if (revealColors && mode != GameMode::HARD)
            setTextColor(TILES[pieceIdx].consoleColor);
        else
            setTextColor(FB_EMPTY);
        std::cout << "[" << TILES[pieceIdx].label << "] ";
    }
    setTextColor(FB_EMPTY);
}

void cleanRowCursor(int row) {
    const int GRID_START_COL = 35;
    const int BOX_STEP = 9;
    for (int i = 0; i < CODE_LENGTH; ++i) {
        setCursorPosition(GRID_START_COL + i * BOX_STEP, row);
        std::cout << "       ";
    }
}

void drawFeedbackOnRow(const std::array<int, CODE_LENGTH>& guess, const std::array<int, CODE_LENGTH>& tileFeedback, int row, GameMode mode) {
    const int GRID_START_COL = 44;
    const int BOX_STEP = 9;
    for (int i = 0; i < CODE_LENGTH; ++i) {
        int displayCol = GRID_START_COL + i * BOX_STEP;
        setCursorPosition(displayCol, row);
        int pieceIdx = guess[i];
        int feedback = tileFeedback[i];
        WORD attr;
        if (mode == GameMode::HARD) {
            attr = TILES[pieceIdx].consoleColor;
        } else {
            if (feedback == 1) attr = FB_GREEN;
            else if (feedback == 2 && mode != GameMode::MEDIUM) attr = FB_YELLOW;
            else attr = FB_RED;
        }
        setTextColor(attr);
        std::cout << "[" << TILES[pieceIdx].label << "] ";
    }
    setTextColor(FB_EMPTY);
}

void showInlineCounts(int correctPlace, int correctColor, int row, GameMode mode, bool showDiagnostics) {
    if (mode == GameMode::HARD) return;
    setCursorPosition(80, row);
    setTextColor(FB_GREEN);
    std::cout << "CP:" << correctPlace << " ";
    if (mode != GameMode::MEDIUM) {
        setTextColor(FB_YELLOW);
        std::cout << "CC:" << correctColor;
    }
    if (showDiagnostics) std::cout << " [OK]";
    setTextColor(FB_EMPTY);
}

void showInlineCounts(int correctPlace, int correctColor, int row, GameMode mode, const std::string& devTelemetry) {
    showInlineCounts(correctPlace, correctColor, row, mode, false);
    std::cout << " | " << devTelemetry;
}

// ============================================================================
// Game Flow Functions
// ============================================================================
GameMode selectDifficulty() {
    clearScreen();
    drawTitleBar();
    setCursorPosition(48, 7);  setTextColor(FB_YELLOW); std::cout << "[ SELECT DIFFICULTY ]";
    setCursorPosition(30, 9);  setTextColor(FB_GREEN);   std::cout << "1 - EASY   (Tiles show correct/wrong colors, full CP+CC shown)";
    setCursorPosition(30, 11); setTextColor(FB_YELLOW);  std::cout << "2 - MEDIUM (Only Green/Red tiles, CP count only)";
    setCursorPosition(30, 13); setTextColor(FB_RED);     std::cout << "3 - HARD   (Classic Mastermind: tile colors never change)";
    setCursorPosition(48, 16); setTextColor(FB_EMPTY);   std::cout << "Press 1, 2, or 3...";

    while (true) {
        char key = static_cast<char>(_getch());
        if (key == '1') return GameMode::EASY;
        if (key == '2') return GameMode::MEDIUM;
        if (key == '3') return GameMode::HARD;
    }
}

void player1EnterCode(GameState& state, std::array<int, CODE_LENGTH>& outCode) {
    resetArrayProfile(outCode, 0);
    int activeTile = 0;
    clearScreen();
    drawTitleBar();
    drawSidePanels(state.mode, true);
    setCursorPosition(54, 5); setTextColor(FB_EMPTY); std::cout << "[ PLAYER 1 ]";
    setCursorPosition(49, 6); std::cout << "Set your secret code!";

    const int CODE_ROW = 12;
    while (true) {
        setCursorPosition(56, 9); setTextColor(FB_EMPTY); std::cout << "Tile: ";
        setTextColor(FB_YELLOW); std::cout << activeTile + 1;
        renderGuessRow(outCode, activeTile, CODE_ROW, GameMode::HARD, false);
        setCursorPosition(0, 24);

        int raw = _getch();
        if (raw == 0 || raw == 224) {
            int arrow = _getch();
            switch (arrow) {
                case 75: activeTile = (activeTile - 1 + CODE_LENGTH) % CODE_LENGTH; break;
                case 77: activeTile = (activeTile + 1) % CODE_LENGTH; break;
                case 72: outCode[activeTile] = (outCode[activeTile] + 1) % PIECE_COUNT; break;
                case 80: outCode[activeTile] = (outCode[activeTile] - 1 + PIECE_COUNT) % PIECE_COUNT; break;
            }
        } else if (raw == 13) {
            cleanRowCursor(CODE_ROW);
            break;
        }
    }
}

void player2GuessLoop(GameState& state, std::vector<std::vector<int>>& sessionHistory) {
    const int GRID_START_ROW = 6;
    state.attemptNumber = 0;
    state.playerTwoWon = false;
    clearScreen();
    drawTitleBar();
    drawSidePanels(state.mode, false);
    drawGrid();
    setCursorPosition(38, 4); setTextColor(FB_EMPTY); std::cout << "PLAYER 2: GUESS THE SECRET CODE!";

    DWORD* timestamps = new DWORD[MAX_TRIES];
    while (state.attemptNumber < MAX_TRIES) {
        timestamps[state.attemptNumber] = GetTickCount();
        resetArrayProfile(state.currentGuess, 0);
        int activeTile = 0;
        int guessRow = GRID_START_ROW + state.attemptNumber * 2;
        setCursorPosition(0, 4);
        std::cout << "                                                                                                                       ";
        setCursorPosition(55, 4); setTextColor(FB_EMPTY);
        std::cout << std::format("TRY {:2}/{}", state.attemptNumber + 1, MAX_TRIES);

        while (true) {
            renderGuessRow(state.currentGuess, activeTile, guessRow, state.mode, true);
            setCursorPosition(0, 24);
            int raw = _getch();
            if (raw == 0 || raw == 224) {
                int arrow = _getch();
                switch (arrow) {
                    case 75: activeTile = (activeTile - 1 + CODE_LENGTH) % CODE_LENGTH; break;
                    case 77: activeTile = (activeTile + 1) % CODE_LENGTH; break;
                    case 72: state.currentGuess[activeTile] = (state.currentGuess[activeTile] + 1) % PIECE_COUNT; break;
                    case 80: state.currentGuess[activeTile] = (state.currentGuess[activeTile] - 1 + PIECE_COUNT) % PIECE_COUNT; break;
                }
            } else if (raw == 13) {
                cleanRowCursor(guessRow);
                break;
            }
        }

        sessionHistory.push_back(std::vector<int>(state.currentGuess.begin(), state.currentGuess.end()));

        const auto& alias = state.currentGuess;
        int cp = 0, cc = 0;
        std::array<int, CODE_LENGTH> fb = {};
        calculateFeedback(state.secretCode, alias, cp, cc, fb);
        drawFeedbackOnRow(state.currentGuess, fb, guessRow, state.mode);
        showInlineCounts(cp, cc, guessRow, state.mode);
        state.lastCorrectPlace = cp;
        state.lastCorrectColor = cc;
        state += 1;

        if (state == state.currentGuess) {
            state.playerTwoWon = true;
            break;
        }
        state.attemptNumber++;
    }

    delete[] timestamps;
    timestamps = nullptr;

    double precise = calculateAccuracy(state.lastCorrectPlace, state.totalAttempts);
    state.accuracyPercent = precise;
    int truncated = static_cast<int>(precise);
    (void)truncated;
}

void displayEndScreen(const GameState& state, const std::string& p2Name) {
    clearScreen();
    showCursor();
    drawTitleBar();

    // Clear central area
    for (int row = 5; row <= 24; ++row) {
        setCursorPosition(20, row);
        for (int col = 0; col < 80; ++col) std::cout << ' ';
    }

    setCursorPosition(52, 5); setTextColor(FB_YELLOW); std::cout << " FINAL RESULTS ";
    setCursorPosition(44, 7);
    if (state.playerTwoWon) {
        setTextColor(FB_GREEN);
        std::cout << "** " << p2Name << " WINS! CODE BROKEN! **";
    } else {
        setTextColor(FB_YELLOW);
        std::cout << "** PLAYER 1 WINS! CODE SECURE! **";
    }
    setCursorPosition(49, 9); setTextColor(FB_YELLOW); std::cout << "SECRET CODE REVEALED:";

    const int REVEAL_ROW = 11, REVEAL_COL = 44, BOX_STEP = 9;
    for (int i = 0; i < CODE_LENGTH; ++i) {
        setCursorPosition(REVEAL_COL + i * BOX_STEP, REVEAL_ROW);
        int p = state.secretCode[i];
        setTextColor(TILES[p].consoleColor);
        std::cout << "[" << TILES[p].label << "] ";
    }
    setCursorPosition(55, 14); setTextColor(FB_YELLOW); std::cout << p2Name << "'s";
    setCursorPosition(55, 15); setTextColor(FB_YELLOW); std::cout <<  "STATISTICS";
    setCursorPosition(52, 17); setTextColor(FB_EMPTY); std::cout << "Guesses Made : " << state.totalAttempts;
    setCursorPosition(52, 18); std::cout << "Correct Color: " << state.lastCorrectColor;
    setCursorPosition(52, 19); std::cout << "Correct Place: " << state.lastCorrectPlace;
    setCursorPosition(52, 20); setTextColor(FB_GREEN); std::cout << std::format("Accuracy: {:.1f}%", state.accuracyPercent);
    setCursorPosition(52, 21); std::cout << "Lifetime Wins: " << g_playerLifetimeWins[p2Name];
    setCursorPosition(51, 23); setTextColor(FB_EMPTY); std::cout << "[R]eplay   [Q]uit";
}

// ============================================================================
// Main Entry Point
// ============================================================================
int main() {
    static int sessionCount = 0;

    clearScreen();
    showCursor();
    setCursorPosition(42, 8);
    setTextColor(FB_YELLOW);
    std::cout << "COLOR CODE GUESSING GAME";

    std::string playerTwoName;
    setCursorPosition(38, 12);
    setTextColor(FB_EMPTY);
    std::cout << "Enter Player 2 Identifier: ";
    std::getline(std::cin, playerTwoName);
    if (playerTwoName.empty()) playerTwoName = "Player 2";

    if (g_playerLifetimeWins.find(playerTwoName) == g_playerLifetimeWins.end())
        g_playerLifetimeWins[playerTwoName] = 0;

    setCursorPosition(0, 28);
    std::cout << "Press any key to start the game...\n";
    _getch();

    hideCursor();

    bool keepPlaying = true;
    while (keepPlaying) {
        sessionCount++;
        std::vector<std::vector<int>> history;
        GameState state;
        resetArrayProfile(state.secretCode, 0);
        resetArrayProfile(state.currentGuess, 0);
        state.attemptNumber = 0;
        state.totalAttempts = 0;
        state.playerTwoWon = false;
        state.lastCorrectPlace = 0;
        state.lastCorrectColor = 0;
        state.accuracyPercent = 0.0;
        state.mode = selectDifficulty();

        clearScreen();
        setCursorPosition(35, 12);
        setTextColor(FB_EMPTY);
        std::cout << "*** PLAYER 1: PRESS ANY KEY TO SET SECRET CODE ***";
        _getch();

        player1EnterCode(state, state.secretCode);

        clearScreen();
        drawTitleBar();
        setCursorPosition(52, 8); setTextColor(FB_YELLOW); std::cout << "** Code SET! **";
        setCursorPosition(56, 10); setTextColor(FB_EMPTY); std::cout << "Pass to ";
        setCursorPosition(56, 11); setTextColor(FB_EMPTY); std::cout << playerTwoName;
        setCursorPosition(52, 13); std::cout << "Press any key...";
        _getch();

        player2GuessLoop(state, history);

        if (state.playerTwoWon)
            g_playerLifetimeWins[playerTwoName]++;

        displayEndScreen(state, playerTwoName);

        bool valid = false;
        do {
            char ch = static_cast<char>(_getch());
            if (ch == 'r' || ch == 'R') { keepPlaying = true; valid = true; }
            else if (ch == 'q' || ch == 'Q') { keepPlaying = false; valid = true; }
        } while (!valid);
    }

    clearScreen();
    showCursor();
    setTextColor(FB_EMPTY);
    return 0;
}
