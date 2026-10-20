#include "raylib.h"
#include <string>
#include <vector>
#include <algorithm>

// Moore FSM based on the lecturer's vending machine diagram.
enum class State { S0, S500, S1000, UB500, UB1000, UA500, UA1000, DROP };
enum class Input { COIN500, COIN1000, PROCESS, CANCEL };

const char* Name(State s) {
    switch (s) {
        case State::S0: return "S0";
        case State::S500: return "S500";
        case State::S1000: return "S1000";
        case State::UB500: return "UB500";
        case State::UB1000: return "UB1000";
        case State::UA500: return "UA500";
        case State::UA1000: return "UA1000";
        case State::DROP: return "DROP";
    }
    return "?";
}

struct Machine {
    State state = State::S0;
    State afterClock = State::S0;
    double clockAt = 0.0;
    int drinks = 0;
    int returned = 0;
    std::string message = "INSERT COIN TO BEGIN";
    std::vector<std::string> history = { "SYSTEM READY : S0" };

    int Balance() const {
        // Refund / drop states have a momentary output; their displayed
        // balance reflects the retained amount (if any).
        if (state == State::S500) return 500;
        if (state == State::S1000 || state == State::UB500 || state == State::UB1000) return 1000;
        return 0;
    }
    bool Busy() const {
        return state == State::UB500 || state == State::UB1000 ||
               state == State::UA500 || state == State::UA1000 || state == State::DROP;
    }
    void Log(const std::string& entry) {
        history.insert(history.begin(), entry);
        if (history.size() > 7) history.pop_back();
    }
    void Move(State next, const std::string& event) {
        std::string old = Name(state);
        state = next;
        Log(old + " > " + Name(next) + " : " + event);
    }
    void Output(State outputState, State destination, const std::string& event, double now) {
        Move(outputState, event);
        afterClock = destination;
        clockAt = now + 1.0; // one visible simulated clock interval
        if (outputState == State::DROP) {
            ++drinks;
            message = "ENJOY YOUR DRINK!";
        } else {
            int amount = (outputState == State::UB500 || outputState == State::UA500) ? 500 : 1000;
            returned += amount;
            message = "RETURNED RP " + std::to_string(amount);
        }
    }
    void Press(Input input, double now) {
        if (Busy()) return; // input locked until next clock tick
        switch (state) {
            case State::S0:
                if (input == Input::COIN500) { Move(State::S500, "+500"); message = "COIN RP 500 ACCEPTED"; }
                else if (input == Input::COIN1000) { Move(State::S1000, "+1000"); message = "COIN RP 1000 ACCEPTED"; }
                else if (input == Input::PROCESS) message = "INSERT RP 1000 FIRST";
                else message = "NOTHING TO CANCEL";
                break;
            case State::S500:
                if (input == Input::COIN500) { Move(State::S1000, "+500"); message = "READY - PRESS PROCESS"; }
                else if (input == Input::COIN1000) Output(State::UB500, State::S1000, "+1000, CHANGE", now);
                else if (input == Input::CANCEL) Output(State::UA500, State::S0, "CANCEL", now);
                else message = "NOT ENOUGH MONEY";
                break;
            case State::S1000:
                if (input == Input::COIN500) Output(State::UB500, State::S1000, "+500, CHANGE", now);
                else if (input == Input::COIN1000) Output(State::UB1000, State::S1000, "+1000, CHANGE", now);
                else if (input == Input::CANCEL) Output(State::UA1000, State::S0, "CANCEL", now);
                else Output(State::DROP, State::S0, "PROCESS", now);
                break;
            default: break;
        }
    }
    void Update(double now) {
        if (Busy() && now >= clockAt) {
            Move(afterClock, "CLOCK");
            message = state == State::S0 ? "INSERT COIN TO BEGIN" : "READY - PRESS PROCESS";
        }
    }
};

static const Color BG = {24, 31, 49, 255};
static const Color PANEL = {37, 49, 72, 255};
static const Color BLACKISH = {15, 22, 36, 255};
static const Color WHITEISH = {239, 241, 224, 255};
static const Color PINK = {238, 119, 151, 255};
static const Color CYAN = {100, 219, 221, 255};
static const Color GREEN = {117, 224, 154, 255};
static const Color YELLOWISH = {255, 212, 103, 255};
static const Color REDISH = {224, 98, 105, 255};

void PixelBox(int x, int y, int w, int h, Color fill, Color outline = BLACKISH) {
    DrawRectangle(x - 2, y - 2, w + 4, h + 4, outline);
    DrawRectangle(x, y, w, h, fill);
}

bool PixelButton(Rectangle r, const char* label, Color fill, Vector2 mouse, bool clicked, bool disabled) {
    bool hover = !disabled && CheckCollisionPointRec(mouse, r);
    Color paint = disabled ? Color{75, 82, 96, 255} : (hover ? Color{(unsigned char)std::min(255, fill.r+25), (unsigned char)std::min(255, fill.g+25), (unsigned char)std::min(255, fill.b+25), 255} : fill);
    PixelBox((int)r.x, (int)r.y + (hover ? 2 : 0), (int)r.width, (int)r.height - 2, paint);
    int size = 12;
    DrawText(label, (int)(r.x + (r.width - MeasureText(label, size))/2), (int)r.y + 10 + (hover ? 2 : 0), size, BLACKISH);
    return hover && clicked;
}

void Drink(int x, int y, Color c) {
    DrawRectangle(x+4, y, 12, 3, WHITEISH);
    DrawRectangle(x+2, y+3, 16, 25, c);
    DrawRectangle(x+5, y+7, 10, 4, WHITEISH);
    DrawRectangle(x+5, y+14, 10, 2, BLACKISH);
    DrawRectangle(x+4, y+25, 12, 3, BLACKISH);
}

void Render(const Machine& m, Vector2 mouse, bool clicked, Input& event, bool& hasEvent) {
    ClearBackground(BG);
    DrawText("PIXEL POP!", 28, 18, 31, PINK);
    DrawText("VENDING MACHINE  /  MOORE FSM", 31, 56, 13, WHITEISH);
    DrawText("PRICE : RP 1000", 482, 28, 13, YELLOWISH);

    // machine
    DrawRectangle(31, 91, 247, 290, BLACKISH);
    DrawRectangle(38, 84, 232, 291, PINK);
    DrawRectangle(44, 90, 220, 7, Color{255, 161, 182, 255});
    DrawText("FRESH SODA", 69, 101, 20, BLACKISH);
    PixelBox(52, 137, 133, 143, BLACKISH);
    DrawRectangle(59, 144, 119, 129, Color{43, 69, 90, 255});
    Color cans[6] = { REDISH, YELLOWISH, GREEN, CYAN, Color{179,139,239,255}, Color{248,153,92,255} };
    for (int i=0; i<6; ++i) Drink(69+(i%3)*35, 155+(i/3)*55, cans[i]);
    DrawRectangle(59, 197, 119, 4, BLACKISH);
    DrawRectangle(59, 252, 119, 4, BLACKISH);
    PixelBox(197, 146, 56, 41, BLACKISH);
    DrawText("RP", 207, 150, 12, GREEN);
    DrawText(TextFormat("%d", m.Balance()), 201, 167, 16, YELLOWISH);
    PixelBox(207, 204, 35, 11, BLACKISH, WHITEISH);
    DrawText("COIN", 204, 220, 10, BLACKISH);
    PixelBox(72, 301, 125, 39, BLACKISH);
    if (m.state == State::DROP) Drink(126, 303, CYAN);
    DrawText("TAKE", 93, 347, 15, BLACKISH);
    DrawRectangle(43, 365, 220, 5, REDISH);

    // right status panel
    PixelBox(304, 87, 304, 112, PANEL);
    DrawText("CURRENT BALANCE", 317, 98, 13, WHITEISH);
    DrawText(TextFormat("RP %d", m.Balance()), 317, 120, 29, YELLOWISH);
    DrawText("STATE", 318, 164, 14, WHITEISH);
    DrawText(Name(m.state), 395, 159, 22, CYAN);
    if (m.Busy()) DrawText("CLOCK...", 513, 174, 11, PINK);

    Rectangle buttons[4] = {{307, 215, 141, 39},{465, 215, 141, 39},{307, 270, 141, 39},{465, 270, 141, 39}};
    const char* names[4] = {"+ RP 500", "+ RP 1000", "PROCESS", "CANCEL"};
    Color fills[4] = {YELLOWISH, CYAN, GREEN, REDISH};
    for (int i=0; i<4; ++i) {
        if (PixelButton(buttons[i], names[i], fills[i], mouse, clicked, m.Busy())) {
            event = (Input)i; hasEvent = true;
        }
    }
    DrawText("[1]  [2]  [ENTER]  [BACKSPACE]", 317, 322, 11, WHITEISH);

    // diagnostic footer (kept deliberately compact to resemble retro games)
    PixelBox(306, 342, 301, 40, BLACKISH);
    DrawText(m.message.c_str(), 316, 351, 12, GREEN);
    DrawText(TextFormat("DRINKS %d    REFUND RP %d", m.drinks, m.returned), 316, 367, 10, WHITEISH);

    DrawText("TRANSITION LOG", 32, 391, 10, WHITEISH);
    // Log is available for the presentation, but only latest entry is
    // shown in the small game interface; use 'L' to toggle fullscreen log.
    DrawText(m.history.front().c_str(), 164, 391, 10, CYAN);
}

int main() {
    const int W = 640, H = 410, SCALE = 2;
    InitWindow(W*SCALE, H*SCALE, "Pixel Pop! - Moore FSM Vending Machine");
    SetTargetFPS(60);
    RenderTexture2D canvas = LoadRenderTexture(W,H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);
    Machine machine;
    bool showLog = false;

    while (!WindowShouldClose()) {
        double now = GetTime();
        machine.Update(now);
        Vector2 mp = GetMousePosition();
        Vector2 mouse = {mp.x/SCALE, mp.y/SCALE};
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        Input event = Input::COIN500;
        bool hasEvent = false;

        if (IsKeyPressed(KEY_ONE)) { event=Input::COIN500; hasEvent=true; }
        else if (IsKeyPressed(KEY_TWO)) { event=Input::COIN1000; hasEvent=true; }
        else if (IsKeyPressed(KEY_ENTER)) { event=Input::PROCESS; hasEvent=true; }
        else if (IsKeyPressed(KEY_BACKSPACE)) { event=Input::CANCEL; hasEvent=true; }
        if (IsKeyPressed(KEY_L)) showLog = !showLog;

        BeginTextureMode(canvas);
        Render(machine, mouse, clicked, event, hasEvent);
        if (showLog) {
            PixelBox(302, 92, 310, 272, PANEL);
            DrawText("FSM TRANSITION HISTORY", 312, 105, 16, YELLOWISH);
            for (int i=0; i<(int)machine.history.size(); ++i)
                DrawText(machine.history[i].c_str(), 313, 139+i*28, 11, WHITEISH);
            DrawText("PRESS L TO CLOSE", 312, 335, 11, CYAN);
        }
        EndTextureMode();

        if (hasEvent && !showLog) machine.Press(event, now);

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(canvas.texture, Rectangle{0,0,(float)W,-(float)H},
            Rectangle{0,0,(float)(W*SCALE),(float)(H*SCALE)}, Vector2{0,0}, 0, WHITE);
        EndDrawing();
    }
    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}
