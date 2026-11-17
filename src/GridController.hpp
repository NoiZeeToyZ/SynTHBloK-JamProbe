#pragma once
// Grid controller profiles: lets the JAM modules (MTX-8, FORGE, DUO, DRUM, MTX-FX, LOOP, COMPOSER, JAM, GLITCH-ECHO)
// run on other 8x8 grid controllers, not only the Native Instruments Maschine JAM.
//
// How it works: the modules keep speaking "JAM space", the MIDI of the Maschine JAM with the Controller Editor
// template "JAM Template 4 Rack" (tools/JAM Template 4 Rack.ncmj): pad note = padBase + col - 8*row (row 0 = top,
// padBase = the module's page, 56 on page 1), scene buttons CC 0-7, group buttons CC 8-15, touch strips CC 48+,
// knob CC 42-45 (push 46), side / bottom buttons CC 16-24 and 112-119; LED colour = the JAM's indexed palette
// (velocity / 4 = hue, velocity % 4 = brightness, white = 68-71). A profile translates a device's MIDI into
// logical events (pad row / col, scene i, group i, button, strip i, knob) and those into JAM-space messages, and
// the modules' JAM-space LED messages back into the device's. The JAM profile does nothing at all: its bytes
// pass straight through (GridPort.hpp sends them untouched), so the JAM path is the old one byte for byte.
//
// Pure C++ (no Rack headers) so dev/gridctl_test.cpp can test it offline. GPL-3.0-or-later like the free singles.
//
// Sources (checked Nov 16 2026; every number below comes from these):
//  - Akai Professional, "APC mini mk2 - Communication Protocol - v1.0" (PDF,
//    https://cdn.inmusicbrands.com/akai/attachments/APC%20mini%20mk2%20-%20Communication%20Protocol%20-%20v1.0.pdf):
//    pads = notes 0x00-0x3F, track buttons 0x64-0x6B (red LED), scene launch 0x70-0x77 (green LED), shift 0x7A
//    (no LED), faders 1-9 = CC 0x30-0x38 on channel 0; pad LED = note-on, channel 0-6 = solid at 10/25/50/65/75/90/100 %
//    (7-15 pulse / blink), velocity = 128-entry palette (table below, copied from the PDF); single LEDs = 90 nn 00 off /
//    01 on / 02 blink; Introduction message F0 47 7F 4F 60 00 04 00 <maj> <min> <bugfix> F7 is sent "before any other
//    device-specific message". The PDF does not number the pad corners: note 0 = bottom-left comes from the APC mini
//    (mk1) layout the mk2 keeps (Bome forum "initial LED mapping summary") and the apc-mini-mk2 npm package
//    (github.com/DJj123dj/apc-mini-mk2: x = note % 8, y = note / 8 from the bottom). Confirm on hardware.
//  - Novation, "Launchpad Mini [MK3] Programmer's Reference Manual", "Launchpad X Programmer's Reference Manual",
//    "Launchpad Pro [MK3] Programmer's Reference Guide" (fael-downloads-prod.focusrite.com): SysEx header
//    F0 00 20 29 02 <0D Mini MK3 | 0C X | 0E Pro MK3>; Programmer / Live mode switch = header 0E <1 = programmer> F7;
//    Programmer layout (manual figure): grid notes 11-88 (11 = bottom-left, 81 = top-left, 18 = bottom-right), top row
//    CC 91-98, right column CC 89, 79 ... 19, logo CC 99; Pro MK3 adds shift CC 90, left column CC 80 ... 10, and two
//    bottom rows CC 101-108 and CC 1-8. LEDs: note-on / CC on channel 1 = static palette colour (2 flash, 3 pulse),
//    the device sends note-on velocity 0 for releases. The palette chart in these manuals has the same index layout
//    as Akai's table above (0-3 greys / white, 5 red, 9 orange, 13 yellow, 21 green, 41 / 45 blue, 53 magenta, 72 red,
//    87 green, 96 amber ...), so one table serves both makers.
//  - Button labels used for the Pro MK3 left column / bottom row (Up, Down, Clear, Duplicate, Quantise, Fixed Length,
//    Play, Record; Record Arm, Mute, Solo ...) are from the device's printed panel, not its manual: check on hardware.
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <cctype>
#include <cstdlib>

namespace gridctl {

// ---------------------------------------------------------------------------------------------- profiles
enum Kind { JAM = 0, APC_MINI_MK2, LP_MINI_MK3, LP_X, LP_PRO_MK3, NUM_KINDS };
static const int AUTO = -1;

struct ProfileInfo {
    const char* id;          // saved in the patch
    const char* label;       // menu text ("(beta)" = never tried on the real controller)
    const char* match[3];    // MIDI device-name pieces (lower case) that pick this profile under Auto
    const char* portHint;    // which MIDI port to choose
    const char* playHint;    // the button that is the JAM's PLAY (runs the module, or the COMPOSER's song)
};
static const ProfileInfo kProfiles[NUM_KINDS] = {
    // Novation: the MIDI interface (programmer mode) is the SECOND USB interface on Windows ("MIDIIN2 (LPX MIDI)" /
    // "MIDIOUT2 (...)"), "... MIDI In / Out" on macOS; the first / "DAW" one is for DAW scripts.
    {"jam", "Maschine JAM", {"maschine jam", nullptr, nullptr}, "Controller Editor template \"JAM Template 4 Rack\"", "PLAY"},
    {"apcminimk2", "APC mini mk2 (beta)", {"apc mini mk2", "apc mini mkii", nullptr}, "the APC mini mk2's first port", "SHIFT + STOP ALL CLIPS"},
    {"lpminimk3", "Launchpad Mini MK3 (beta)", {"lpminimk3", "launchpad mini mk3", nullptr}, "Windows: MIDIIN2 / MIDIOUT2 (LPMiniMK3 MIDI); Mac: LPMiniMK3 MIDI", "Session (top row, 5th)"},
    {"lpx", "Launchpad X (beta)", {"lpx midi", "lpx daw", "launchpad x"}, "Windows: MIDIIN2 / MIDIOUT2 (LPX MIDI); Mac: LPX MIDI", "Session (top row, 5th)"},
    {"lppromk3", "Launchpad Pro MK3 (beta)", {"lppromk3", "launchpad pro mk3", nullptr}, "Windows: MIDIIN2 / MIDIOUT2 (LPProMK3 MIDI); Mac: LPProMK3 MIDI", "Play (left column, 7th)"},
};
inline const char* kindId(int k) { return k == AUTO ? "auto" : (k >= 0 && k < NUM_KINDS ? kProfiles[k].id : "jam"); }
inline const char* kindLabel(int k) { return k == AUTO ? "Auto (by MIDI device name)" : (k >= 0 && k < NUM_KINDS ? kProfiles[k].label : "?"); }
// a saved id -> kind; unknown or missing = the JAM (old patches)
inline int kindFromId(const char* s) {
    if (!s) return JAM;
    if (!strcmp(s, "auto")) return AUTO;
    for (int k = 0; k < NUM_KINDS; k++) if (!strcmp(s, kProfiles[k].id)) return k;
    return JAM;
}
// Auto: the profile whose name pieces appear in the MIDI device name, else -1 (the caller falls back to the JAM)
inline int kindFromDeviceName(const std::string& name) {
    std::string n;
    for (char ch : name) n += (char)std::tolower((unsigned char)ch);
    for (int k = NUM_KINDS - 1; k >= 0; k--)
        for (const char* m : kProfiles[k].match)
            if (m && n.find(m) != std::string::npos) return k;
    return -1;
}

// ---------------------------------------------------------------------------------------------- JAM space
namespace jam {
// "JAM Template 4 Rack" numbers (same as Matrix8_ui.hpp / SynTHBloKJam.cpp)
enum { SONG = 16, CLEAR = 19, DUPLICATE = 20, UP = 21, LEFT = 22, RIGHT = 23, DOWN = 24,
       PLAY = 112, REC = 113, PREV = 114, NEXT = 115, TEMPO = 116, GRID = 117, SOLO = 118, MUTE = 119 };
static const int kKnobFirstCC = 42, kKnobLastCC = 45, kKnobPushCC = 46;
inline const char* buttonName(int cc) {
    switch (cc) {
        case SONG: return "SONG"; case CLEAR: return "CLEAR"; case DUPLICATE: return "DUPLICATE"; case UP: return "UP";
        case LEFT: return "LEFT"; case RIGHT: return "RIGHT"; case DOWN: return "DOWN"; case PLAY: return "PLAY";
        case REC: return "REC"; case PREV: return "PREV"; case NEXT: return "NEXT"; case TEMPO: return "TEMPO";
        case GRID: return "GRID"; case SOLO: return "SOLO"; case MUTE: return "MUTE";
    }
    return nullptr;
}
}  // namespace jam

// A small MIDI message (3-byte channel messages, or a connect SysEx up to 16 bytes)
struct Msg {
    uint8_t b[16];
    uint8_t n = 0;
    Msg() {}
    Msg(uint8_t a, uint8_t c, uint8_t d) { b[0] = a; b[1] = c; b[2] = d; n = 3; }
    bool operator==(const Msg& o) const { return n == o.n && !memcmp(b, o.b, n); }
};
struct MsgList {
    Msg m[24];
    int n = 0;
    void add(const Msg& x) { if (n < 24) m[n++] = x; }
};

// ---------------------------------------------------------------------------------------------- logical events
enum Target : int16_t {
    T_NONE = 0,
    T_SCENE = 100,    // + 0..7 : JAM scene buttons (CC 0-7)
    T_GROUP = 200,    // + 0..7 : JAM group buttons A-H (CC 8-15)
    T_STRIP = 300,    // + 0..7 : JAM touch strips (CC 48-55, strip page 1)
    T_BTN = 400,      // + JAM CC : side / bottom buttons (jam::SONG ...)
    T_KNOB = 600,     // the JAM knob (CC 42, MST page)
    T_KNOBPUSH = 601, // the knob push (CC 46)
    T_SHIFT = 700,    // the profile's own shift (not sent to the modules)
};
struct Event {
    enum Type { NONE, PAD, CONTROL } type = NONE;
    int row = 0, col = 0;        // PAD: row 0 = top, col 0 = left
    int target = T_NONE;         // CONTROL
    int value = 0;               // velocity (PAD) or 0-127 (CONTROL); 0 = release
    bool noteOff = false;        // PAD released with a note-off (0x80) rather than note-on velocity 0
};

// JAM-space message for an event (what the JAM itself would have sent)
inline bool eventToJam(const Event& e, int padBase, Msg* out) {
    if (e.type == Event::PAD) {
        int note = padBase + e.col - 8 * e.row;
        if (note < 0 || note > 127) return false;
        *out = Msg(e.noteOff ? 0x80 : 0x90, (uint8_t)note, (uint8_t)e.value);
        return true;
    }
    if (e.type != Event::CONTROL) return false;
    int t = e.target, cc = -1;
    if (t >= T_SCENE && t < T_SCENE + 8) cc = t - T_SCENE;
    else if (t >= T_GROUP && t < T_GROUP + 8) cc = 8 + t - T_GROUP;
    else if (t >= T_STRIP && t < T_STRIP + 8) cc = 48 + t - T_STRIP;
    else if (t >= T_BTN && t < T_BTN + 128) cc = t - T_BTN;
    else if (t == T_KNOB) cc = jam::kKnobFirstCC;
    else if (t == T_KNOBPUSH) cc = jam::kKnobPushCC;
    if (cc < 0) return false;
    *out = Msg(0xB0, (uint8_t)cc, (uint8_t)e.value);
    return true;
}

// What a JAM-space LED message lights (the inverse view, for the modules' outgoing messages)
struct Led {
    enum Type { NONE, PAD, SCENE, GROUP, BUTTON, STRIP, KNOB } type = NONE;
    int row = 0, col = 0, index = 0;   // PAD row/col; SCENE / GROUP / STRIP index; BUTTON = JAM CC
    int value = 0;                     // JAM palette index (pads, scenes, groups) or brightness / level
};
inline Led jamToLed(const uint8_t* b, int n, int padBase) {
    Led l;
    if (n < 3) return l;
    int st = b[0] & 0xf0, a = b[1], v = b[2];
    if (st == 0x90 || st == 0x80) {
        int d = a - padBase + 56;   // 0..63 on this page
        if (d < 0 || d > 63) return l;   // the Template 1 Live notes (91-95) above the grid: no LED elsewhere
        l.type = Led::PAD; l.row = 7 - d / 8; l.col = d % 8; l.value = st == 0x80 ? 0 : v;
        return l;
    }
    if (st != 0xB0) return l;
    l.value = v;
    if (a < 8) { l.type = Led::SCENE; l.index = a; }
    else if (a < 16) { l.type = Led::GROUP; l.index = a - 8; }
    else if (jam::buttonName(a)) { l.type = Led::BUTTON; l.index = a; }
    else if (a >= jam::kKnobFirstCC && a <= jam::kKnobLastCC) l.type = Led::KNOB;
    else if (a >= 48 && a < 112) { l.type = Led::STRIP; l.index = (a - 48) % 8; }
    return l;   // DUO's OLED CCs (17, 18, 25-36) stay NONE: only the JAM has those buttons
}

// ---------------------------------------------------------------------------------------------- colours
// Akai APC mini mk2 palette, velocity 0-127 -> RGB (Communication Protocol v1.0, "Velocity to RGB Color Chart";
// the Launchpad MK3 / X palette chart has the same index layout).
static const uint32_t kPalette[128] = {
    0x000000,0x1E1E1E,0x7F7F7F,0xFFFFFF,0xFF4C4C,0xFF0000,0x590000,0x190000,0xFFBD6C,0xFF5400,0x591D00,0x271B00,
    0xFFFF4C,0xFFFF00,0x595900,0x191900,0x88FF4C,0x54FF00,0x1D5900,0x142B00,0x4CFF4C,0x00FF00,0x005900,0x001900,
    0x4CFF5E,0x00FF19,0x00590D,0x001902,0x4CFF88,0x00FF55,0x00591D,0x001F12,0x4CFFB7,0x00FF99,0x005935,0x001912,
    0x4CC3FF,0x00A9FF,0x004152,0x001019,0x4C88FF,0x0055FF,0x001D59,0x000819,0x4C4CFF,0x0000FF,0x000059,0x000019,
    0x874CFF,0x5400FF,0x190064,0x0F0030,0xFF4CFF,0xFF00FF,0x590059,0x190019,0xFF4C87,0xFF0054,0x59001D,0x220013,
    0xFF1500,0x993500,0x795100,0x436400,0x033900,0x005735,0x00547F,0x0000FF,0x00454F,0x2500CC,0x7F7F7F,0x202020,
    0xFF0000,0xBDFF2D,0xAFED06,0x64FF09,0x108B00,0x00FF87,0x00A9FF,0x002AFF,0x3F00FF,0x7A00FF,0xB21A7D,0x402100,
    0xFF4A00,0x88E106,0x72FF15,0x00FF00,0x3BFF26,0x59FF71,0x38FFCC,0x5B8AFF,0x3151C6,0x877FE9,0xD31DFF,0xFF005D,
    0xFF7F00,0xB9B000,0x90FF00,0x835D07,0x392B00,0x144C10,0x0D5038,0x15152A,0x16205A,0x693C1C,0xA8000A,0xDE513D,
    0xD86A1C,0xFFE126,0x9EE12F,0x67B50F,0x1E1E30,0xDCFF6B,0x80FFBD,0x9A99FF,0x8E66FF,0x404040,0x757575,0xE0FFFF,
    0xA00000,0x350000,0x1AD000,0x074200,0xB9B000,0x3F3100,0xB35F00,0x4B1502};

// The JAM's indexed palette: velocity / 4 = hue (0 = off, 1 red ... 17 white), velocity % 4 = brightness (3 = full).
// Hues 1-5 and white are the ones the modules use (Matrix8_ui.hpp: red 4-7, orange 8-11, light orange 12-15, warm
// yellow 16-19, yellow 20-23, white 68-71); their RGB follows the panel colours they stand for. 6-16 follow NI's
// 16-colour order (lime ... fuchsia) and only show up through MTX-8's custom LED colour sliders.
static const uint32_t kJamHue[18] = {
    0x000000, 0xFF1000, 0xFF5000, 0xFF8000, 0xFFB000, 0xFFE000, 0xA0FF00, 0x00FF00, 0x00FF80,
    0x00FFFF, 0x00A0FF, 0x0000FF, 0x6000FF, 0xA000FF, 0xE000FF, 0xFF00C0, 0xFF0060, 0xFFFFFF};
static const float kJamLevel[4] = {0.12f, 0.3f, 0.6f, 1.f};

inline uint32_t scaleRgb(uint32_t c, float f) {
    int r = (int)std::lround(((c >> 16) & 255) * f), g = (int)std::lround(((c >> 8) & 255) * f), b = (int)std::lround((c & 255) * f);
    return (uint32_t)(r << 16 | g << 8 | b);
}
inline int nearestPalette(uint32_t c, bool allowOff) {
    int best = allowOff ? 0 : 1;
    long bestD = -1;
    for (int i = allowOff ? 0 : 1; i < 128; i++) {
        long dr = (long)((kPalette[i] >> 16) & 255) - (long)((c >> 16) & 255);
        long dg = (long)((kPalette[i] >> 8) & 255) - (long)((c >> 8) & 255);
        long db = (long)(kPalette[i] & 255) - (long)(c & 255);
        long d = 3 * dr * dr + 4 * dg * dg + 2 * db * db;   // weighted RGB distance
        if (bestD < 0 || d < bestD) { bestD = d; best = i; }
    }
    return best;
}
inline int jamHue(int v) { return v <= 0 ? 0 : (v / 4 > 17 ? 17 : v / 4); }
inline int jamLevel(int v) { return v > 71 ? 3 : v % 4; }
// JAM palette value -> Launchpad / APC palette index (0 stays off; anything lit stays lit)
inline int paletteForJam(int v) {
    int h = jamHue(v);
    if (h == 0) return 0;
    return nearestPalette(scaleRgb(kJamHue[h], kJamLevel[jamLevel(v)]), false);
}
// JAM palette value -> APC full-brightness colour (the brightness goes in the MIDI channel instead)
inline int apcHueForJam(int v) {
    int h = jamHue(v);
    return h == 0 ? 0 : nearestPalette(kJamHue[h], false);
}
// brightness 0..3 -> APC note-on status (channel 0 = 10 %, 1 = 25 %, 3 = 65 %, 6 = 100 %)
inline uint8_t apcStatusForLevel(int lv) { static const uint8_t s[4] = {0x90, 0x91, 0x93, 0x96}; return s[lv & 3]; }
// a JAM button light (0 off .. 127 full, one colour) -> Launchpad palette, in the panels' amber
inline int paletteForButton(int v) {
    if (v <= 0) return 0;
    return nearestPalette(scaleRgb(0xFF8000, std::fmax(0.12f, v / 127.f)), false);
}
// single-colour LEDs (APC track / scene buttons): JAM palette value -> lit? Dim (brightness 0-1) = off, so the
// selected scene / group stands out the way it does on the JAM (kJamBright vs kJamDim).
inline bool singleOnForJam(int v) { return v > 0 && (v > 71 || v % 4 >= 2); }

// ---------------------------------------------------------------------------------------------- device bindings
enum CtlType : uint8_t { C_NOTE = 0, C_CC = 1 };
struct Bind {
    uint8_t type;    // C_NOTE / C_CC
    uint8_t num;     // note or CC number on the device
    int16_t normal;  // target without shift
    int16_t shifted; // target with the profile's shift held (T_NONE = nothing)
};
#define GC_B(t, n, a, s) {t, n, (int16_t)(a), (int16_t)(s)}
// APC mini mk2: scenes = scene launch, groups (strip banks / LOOP + COMPOSER row mutes) = track buttons, strips =
// faders 1-8, knob = fader 9; SHIFT + the labelled soft keys = the JAM's side buttons. SHIFT + DRUM / NOTE are left
// alone: the APC's own firmware uses them to change its pad mode.
static const Bind kApcBinds[] = {
    GC_B(C_NOTE, 0x70, T_SCENE + 0, T_BTN + jam::CLEAR),      // CLIP STOP
    GC_B(C_NOTE, 0x71, T_SCENE + 1, T_BTN + jam::SOLO),       // SOLO
    GC_B(C_NOTE, 0x72, T_SCENE + 2, T_BTN + jam::REC),        // REC ARM
    GC_B(C_NOTE, 0x73, T_SCENE + 3, T_BTN + jam::MUTE),       // MUTE
    GC_B(C_NOTE, 0x74, T_SCENE + 4, T_BTN + jam::GRID),       // SELECT
    GC_B(C_NOTE, 0x75, T_SCENE + 5, T_NONE),                  // DRUM (firmware)
    GC_B(C_NOTE, 0x76, T_SCENE + 6, T_NONE),                  // NOTE (firmware)
    GC_B(C_NOTE, 0x77, T_SCENE + 7, T_BTN + jam::PLAY),       // STOP ALL CLIPS
    GC_B(C_NOTE, 0x64, T_GROUP + 0, T_BTN + jam::PREV),       // VOLUME
    GC_B(C_NOTE, 0x65, T_GROUP + 1, T_BTN + jam::NEXT),       // PAN
    GC_B(C_NOTE, 0x66, T_GROUP + 2, T_BTN + jam::DUPLICATE),  // SEND
    GC_B(C_NOTE, 0x67, T_GROUP + 3, T_BTN + jam::TEMPO),      // DEVICE
    GC_B(C_NOTE, 0x68, T_GROUP + 4, T_BTN + jam::UP),         // up arrow
    GC_B(C_NOTE, 0x69, T_GROUP + 5, T_BTN + jam::DOWN),       // down arrow
    GC_B(C_NOTE, 0x6A, T_GROUP + 6, T_BTN + jam::LEFT),       // left arrow
    GC_B(C_NOTE, 0x6B, T_GROUP + 7, T_BTN + jam::RIGHT),      // right arrow
    GC_B(C_NOTE, 0x7A, T_SHIFT, T_SHIFT),                     // SHIFT
    GC_B(C_CC, 0x30, T_STRIP + 0, T_STRIP + 0), GC_B(C_CC, 0x31, T_STRIP + 1, T_STRIP + 1),
    GC_B(C_CC, 0x32, T_STRIP + 2, T_STRIP + 2), GC_B(C_CC, 0x33, T_STRIP + 3, T_STRIP + 3),
    GC_B(C_CC, 0x34, T_STRIP + 4, T_STRIP + 4), GC_B(C_CC, 0x35, T_STRIP + 5, T_STRIP + 5),
    GC_B(C_CC, 0x36, T_STRIP + 6, T_STRIP + 6), GC_B(C_CC, 0x37, T_STRIP + 7, T_STRIP + 7),
    GC_B(C_CC, 0x38, T_KNOB, T_KNOB),                         // fader 9 (master)
};
// Launchpad Mini MK3 / X: right column = scenes (with the top row's last button held as SHIFT: groups A-H),
// top row = arrows + PLAY / REC / CLEAR (with SHIFT: SOLO / MUTE / PREV / NEXT / TEMPO / GRID / DUPLICATE).
static const Bind kLpBinds[] = {
    GC_B(C_CC, 89, T_SCENE + 0, T_GROUP + 0), GC_B(C_CC, 79, T_SCENE + 1, T_GROUP + 1),
    GC_B(C_CC, 69, T_SCENE + 2, T_GROUP + 2), GC_B(C_CC, 59, T_SCENE + 3, T_GROUP + 3),
    GC_B(C_CC, 49, T_SCENE + 4, T_GROUP + 4), GC_B(C_CC, 39, T_SCENE + 5, T_GROUP + 5),
    GC_B(C_CC, 29, T_SCENE + 6, T_GROUP + 6), GC_B(C_CC, 19, T_SCENE + 7, T_GROUP + 7),
    GC_B(C_CC, 91, T_BTN + jam::UP, T_BTN + jam::SOLO),        // up arrow
    GC_B(C_CC, 92, T_BTN + jam::DOWN, T_BTN + jam::MUTE),      // down arrow
    GC_B(C_CC, 93, T_BTN + jam::LEFT, T_BTN + jam::PREV),      // left arrow
    GC_B(C_CC, 94, T_BTN + jam::RIGHT, T_BTN + jam::NEXT),     // right arrow
    GC_B(C_CC, 95, T_BTN + jam::PLAY, T_BTN + jam::TEMPO),     // Session
    GC_B(C_CC, 96, T_BTN + jam::REC, T_BTN + jam::GRID),       // Drums (Mini) / Note (X)
    GC_B(C_CC, 97, T_BTN + jam::CLEAR, T_BTN + jam::DUPLICATE),// Keys (Mini) / Custom (X)
    GC_B(C_CC, 98, T_SHIFT, T_SHIFT),                          // User (Mini) / Capture MIDI (X)
};
// Launchpad Pro MK3: enough buttons for everything without a shift layer.
static const Bind kLpProBinds[] = {
    GC_B(C_CC, 89, T_SCENE + 0, T_SCENE + 0), GC_B(C_CC, 79, T_SCENE + 1, T_SCENE + 1),
    GC_B(C_CC, 69, T_SCENE + 2, T_SCENE + 2), GC_B(C_CC, 59, T_SCENE + 3, T_SCENE + 3),
    GC_B(C_CC, 49, T_SCENE + 4, T_SCENE + 4), GC_B(C_CC, 39, T_SCENE + 5, T_SCENE + 5),
    GC_B(C_CC, 29, T_SCENE + 6, T_SCENE + 6), GC_B(C_CC, 19, T_SCENE + 7, T_SCENE + 7),
    GC_B(C_CC, 101, T_GROUP + 0, T_GROUP + 0), GC_B(C_CC, 102, T_GROUP + 1, T_GROUP + 1),
    GC_B(C_CC, 103, T_GROUP + 2, T_GROUP + 2), GC_B(C_CC, 104, T_GROUP + 3, T_GROUP + 3),
    GC_B(C_CC, 105, T_GROUP + 4, T_GROUP + 4), GC_B(C_CC, 106, T_GROUP + 5, T_GROUP + 5),
    GC_B(C_CC, 107, T_GROUP + 6, T_GROUP + 6), GC_B(C_CC, 108, T_GROUP + 7, T_GROUP + 7),
    GC_B(C_CC, 80, T_BTN + jam::UP, T_BTN + jam::UP),               // Up
    GC_B(C_CC, 70, T_BTN + jam::DOWN, T_BTN + jam::DOWN),           // Down
    GC_B(C_CC, 60, T_BTN + jam::CLEAR, T_BTN + jam::CLEAR),         // Clear
    GC_B(C_CC, 50, T_BTN + jam::DUPLICATE, T_BTN + jam::DUPLICATE), // Duplicate
    GC_B(C_CC, 40, T_BTN + jam::GRID, T_BTN + jam::GRID),           // Quantise
    GC_B(C_CC, 30, T_BTN + jam::SONG, T_BTN + jam::SONG),           // Fixed Length
    GC_B(C_CC, 20, T_BTN + jam::PLAY, T_BTN + jam::PLAY),           // Play
    GC_B(C_CC, 10, T_BTN + jam::REC, T_BTN + jam::REC),             // Record
    GC_B(C_CC, 91, T_BTN + jam::LEFT, T_BTN + jam::LEFT),           // left arrow
    GC_B(C_CC, 92, T_BTN + jam::RIGHT, T_BTN + jam::RIGHT),         // right arrow
    GC_B(C_CC, 93, T_BTN + jam::PREV, T_BTN + jam::PREV),           // Session
    GC_B(C_CC, 94, T_BTN + jam::NEXT, T_BTN + jam::NEXT),           // Note
    GC_B(C_CC, 95, T_BTN + jam::TEMPO, T_BTN + jam::TEMPO),         // Chord
    GC_B(C_CC, 2, T_BTN + jam::MUTE, T_BTN + jam::MUTE),            // Mute
    GC_B(C_CC, 3, T_BTN + jam::SOLO, T_BTN + jam::SOLO),            // Solo
};
#undef GC_B
inline const Bind* binds(int kind, int* n) {
    switch (kind) {
        case APC_MINI_MK2: *n = (int)(sizeof(kApcBinds) / sizeof(Bind)); return kApcBinds;
        case LP_MINI_MK3: case LP_X: *n = (int)(sizeof(kLpBinds) / sizeof(Bind)); return kLpBinds;
        case LP_PRO_MK3: *n = (int)(sizeof(kLpProBinds) / sizeof(Bind)); return kLpProBinds;
    }
    *n = 0; return nullptr;
}
// the device control that shows a target's light (its unshifted binding), or -1
inline int bindFor(int kind, int target, uint8_t* type) {
    int n; const Bind* b = binds(kind, &n);
    for (int i = 0; i < n; i++) if (b[i].normal == target) { *type = b[i].type; return b[i].num; }
    return -1;
}

// grid: device note <-> row (0 = top) / col
inline bool isLaunchpad(int kind) { return kind == LP_MINI_MK3 || kind == LP_X || kind == LP_PRO_MK3; }
inline int gridNote(int kind, int r, int c) {
    if (kind == APC_MINI_MK2) return (7 - r) * 8 + c;
    if (isLaunchpad(kind)) return (8 - r) * 10 + c + 1;
    return -1;
}
inline bool gridPos(int kind, int note, int* r, int* c) {
    if (kind == APC_MINI_MK2) { if (note < 0 || note > 63) return false; *r = 7 - note / 8; *c = note % 8; return true; }
    if (isLaunchpad(kind)) {
        int row = note / 10, col = note % 10;
        if (row < 1 || row > 8 || col < 1 || col > 8) return false;
        *r = 8 - row; *c = col - 1; return true;
    }
    return false;
}

// connect messages: put the device where the profile expects it
inline void connectMsgs(int kind, MsgList* out) {
    Msg m;
    switch (kind) {
        case APC_MINI_MK2: {   // Introduction message, application version 1.0.0
            static const uint8_t s[] = {0xF0, 0x47, 0x7F, 0x4F, 0x60, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0xF7};
            memcpy(m.b, s, sizeof s); m.n = sizeof s; out->add(m);
        } break;
        case LP_MINI_MK3: case LP_X: case LP_PRO_MK3: {   // Programmer / Live mode switch -> Programmer
            uint8_t dev = kind == LP_MINI_MK3 ? 0x0D : kind == LP_X ? 0x0C : 0x0E;
            const uint8_t s[] = {0xF0, 0x00, 0x20, 0x29, 0x02, dev, 0x0E, 0x01, 0xF7};
            memcpy(m.b, s, sizeof s); m.n = sizeof s; out->add(m);
            if (kind != LP_PRO_MK3) out->add(Msg(0xB0, 98, 1));   // SHIFT (top-right button) shows dim grey
        } break;
    }
}

// ---------------------------------------------------------------------------------------------- translator
// One per module. In: device MIDI -> JAM-space MIDI. Out: the module's JAM-space LED messages -> device MIDI.
// Buttons with a SHIFT function show that function's light while SHIFT is held (e.g. PLAY on the APC's STOP ALL
// CLIPS), so the translator keeps the last light of every scene / group / button and repaints on SHIFT.
struct Translator {
    int kind = JAM;
    bool shift = false;
    int16_t held[2][128];   // target each pressed device control went to, so its release goes to the same one
    int16_t light[800];     // last JAM-space light per target (-1 = never sent)
    // SOFT TAKEOVER (Gemini's review): a fader (APC strips 1-8, knob = fader 9) only takes over once it reaches the
    // module's value (the strip / knob light the module sends back), so moving it never jumps a value
    int16_t faderPos[9], faderSent[9];
    bool picked[9];
    static int faderIdx(int t) { return t == T_KNOB ? 8 : t - T_STRIP; }
    bool pickup(int t, int v) {
        int i = faderIdx(t), c = light[t];
        if (c < 0) picked[i] = true;   // the module never said: follow the fader
        else if (!picked[i] && (std::abs(v - c) <= 4 || (faderPos[i] >= 0 && (faderPos[i] - c) * (v - c) <= 0))) picked[i] = true;
        faderPos[i] = (int16_t)v;
        if (picked[i]) faderSent[i] = (int16_t)v;
        return picked[i];
    }
    Translator() { reset(); }
    void reset() {
        shift = false;
        for (auto& t : held) for (auto& x : t) x = T_NONE;
        for (auto& x : light) x = -1;
        for (int i = 0; i < 9; i++) { faderPos[i] = -1; faderSent[i] = -1; picked[i] = true; }
    }

    // device MIDI -> logical event (false = nothing for the modules). A SHIFT change fills `repaint`.
    bool toEvent(const uint8_t* b, int n, Event* e, MsgList* repaint = nullptr) {
        if (kind == JAM || n < 3) return false;
        int st = b[0] & 0xf0, a = b[1] & 127, v = b[2] & 127;
        bool isNote = st == 0x90 || st == 0x80;
        if (!isNote && st != 0xB0) return false;   // aftertouch, pitch bend ...
        bool press = st != 0x80 && v > 0;
        int r, c;
        if (isNote && gridPos(kind, a, &r, &c)) {
            e->type = Event::PAD; e->row = r; e->col = c; e->value = st == 0x80 ? 0 : v; e->noteOff = st == 0x80;
            return true;
        }
        uint8_t type = isNote ? C_NOTE : C_CC;
        int nb; const Bind* bs = binds(kind, &nb);
        for (int i = 0; i < nb; i++) {
            if (bs[i].type != type || bs[i].num != a) continue;
            if (bs[i].normal == T_SHIFT) {
                if (shift != press) { shift = press; if (repaint) repaintAll(repaint); }
                return false;
            }
            int t;
            bool isFader = type == C_CC && (bs[i].normal == T_KNOB || (bs[i].normal >= T_STRIP && bs[i].normal < T_STRIP + 8));
            if (isFader) { t = bs[i].normal; if (!pickup(t, v)) return false; }   // absolute values, no press / release
            else if (press) { t = shift ? bs[i].shifted : bs[i].normal; held[type][a] = (int16_t)t; }
            else { t = held[type][a]; held[type][a] = T_NONE; }
            if (t == T_NONE) return false;
            e->type = Event::CONTROL; e->target = t;
            e->value = isFader ? v : (press ? 127 : 0);
            return true;
        }
        return false;
    }
    // device MIDI -> JAM-space MIDI for the modules
    bool toJam(const uint8_t* b, int n, int padBase, Msg* out, MsgList* repaint = nullptr) {
        Event e;
        return toEvent(b, n, &e, repaint) && eventToJam(e, padBase, out);
    }

    // the light a bound device button shows now (its target in the current SHIFT layer)
    Msg bindLight(const Bind& bd) const {
        int t = shift ? bd.shifted : bd.normal;
        int v = t > T_NONE && t < 800 && light[t] > 0 ? light[t] : 0;
        bool isBtn = t >= T_BTN && t < T_BTN + 128;
        uint8_t st = bd.type == C_CC ? 0xB0 : 0x90;
        if (kind == APC_MINI_MK2) return Msg(0x90, bd.num, (isBtn ? v > 0 : singleOnForJam(v)) ? 1 : 0);
        return Msg(st, bd.num, (uint8_t)(isBtn ? paletteForButton(v) : paletteForJam(v)));
    }
    static bool hasLight(int kind, const Bind& bd) {
        if (bd.normal == T_SHIFT || bd.normal == T_KNOB) return false;
        if (bd.normal >= T_STRIP && bd.normal < T_STRIP + 8) return false;   // faders
        (void)kind;
        return true;
    }
    void repaintAll(MsgList* out) {
        int nb; const Bind* bs = binds(kind, &nb);
        for (int i = 0; i < nb; i++)
            if (hasLight(kind, bs[i]) && bs[i].normal != bs[i].shifted) {
                out->add(bindLight(bs[i]));
            }
    }
    // a module's JAM-space LED message -> the device's (0 or more messages)
    void ledToDevice(const uint8_t* b, int n, int padBase, MsgList* out) {
        if (kind == JAM) { Msg m; int k = n > 16 ? 16 : n; memcpy(m.b, b, k); m.n = (uint8_t)k; out->add(m); return; }
        Led l = jamToLed(b, n, padBase);
        int v = l.value;
        int t = T_NONE;
        switch (l.type) {
            case Led::PAD: {
                int note = gridNote(kind, l.row, l.col);
                if (kind == APC_MINI_MK2) out->add(v > 0 ? Msg(apcStatusForLevel(jamLevel(v)), (uint8_t)note, (uint8_t)apcHueForJam(v)) : Msg(0x90, (uint8_t)note, 0));
                else out->add(Msg(0x90, (uint8_t)note, (uint8_t)paletteForJam(v)));
                return;
            }
            case Led::SCENE: t = T_SCENE + l.index; break;
            case Led::GROUP: t = T_GROUP + l.index; break;
            case Led::BUTTON: t = T_BTN + l.index; break;
            case Led::STRIP: case Led::KNOB: {   // no lights for these on the devices: kept for SOFT TAKEOVER
                t = l.type == Led::KNOB ? T_KNOB : T_STRIP + l.index;
                int i = faderIdx(t);
                light[t] = (int16_t)v;
                // a value the fader did not just send (bank change, panel knob, patch load): pick it up again
                if (faderSent[i] < 0 || std::abs(v - faderSent[i]) > 8) picked[i] = false;
                return;
            }
            default: return;
        }
        light[t] = (int16_t)v;
        int nb; const Bind* bs = binds(kind, &nb);
        for (int i = 0; i < nb; i++)
            if (hasLight(kind, bs[i]) && (shift ? bs[i].shifted : bs[i].normal) == t) out->add(bindLight(bs[i]));
    }
};
// stateless form (no SHIFT layer): what a light looks like with SHIFT up
inline void ledToDevice(int kind, const uint8_t* b, int n, int padBase, MsgList* out) {
    Translator tr; tr.kind = kind; tr.ledToDevice(b, n, padBase, out);
}

// one line for a device message (JAM PROBE's decoder)
inline std::string describe(int kind, const uint8_t* b, int n) {
    if (n < 1) return "";
    char buf[96];
    if (b[0] == 0xF0) {
        MsgList c; connectMsgs(kind, &c);
        if (c.n && c.m[0].n == n && !memcmp(c.m[0].b, b, n)) return "connect SysEx";
        return "SysEx";
    }
    if (n < 3) return "";
    int st = b[0] & 0xf0, ch = b[0] & 15, a = b[1], v = b[2];
    if (kind == JAM) {
        Led l = jamToLed(b, n, 56);
        if (l.type == Led::PAD) { snprintf(buf, sizeof buf, "pad r%d c%d (page 1) %d", l.row + 1, l.col + 1, v); return buf; }
        if (l.type == Led::SCENE) { snprintf(buf, sizeof buf, "scene %d", l.index + 1); return buf; }
        if (l.type == Led::GROUP) { snprintf(buf, sizeof buf, "group %c", 'A' + l.index); return buf; }
        if (l.type == Led::BUTTON) return jam::buttonName(l.index);
        if (l.type == Led::STRIP) { snprintf(buf, sizeof buf, "strip %d", l.index + 1); return buf; }
        if (l.type == Led::KNOB) return "knob";
        return "";
    }
    int r, c;
    if ((st == 0x90 || st == 0x80) && gridPos(kind, a, &r, &c)) {
        if (kind == APC_MINI_MK2 && st == 0x90 && ch <= 6) {
            static const int pct[7] = {10, 25, 50, 65, 75, 90, 100};
            snprintf(buf, sizeof buf, "pad r%d c%d colour %d #%06X %d%%", r + 1, c + 1, v, (unsigned)kPalette[v & 127], pct[ch]);
        } else snprintf(buf, sizeof buf, "pad r%d c%d %s %d #%06X", r + 1, c + 1, st == 0x80 || !v ? "off" : "on", v, (unsigned)kPalette[v & 127]);
        return buf;
    }
    int nb; const Bind* bs = binds(kind, &nb);
    for (int i = 0; i < nb; i++) {
        if (bs[i].num != a || bs[i].type != (st == 0xB0 ? C_CC : C_NOTE)) continue;
        auto tname = [&](int t) -> std::string {
            char tb[24];
            if (t >= T_SCENE && t < T_SCENE + 8) { snprintf(tb, sizeof tb, "scene %d", t - T_SCENE + 1); return tb; }
            if (t >= T_GROUP && t < T_GROUP + 8) { snprintf(tb, sizeof tb, "group %c", 'A' + t - T_GROUP); return tb; }
            if (t >= T_STRIP && t < T_STRIP + 8) { snprintf(tb, sizeof tb, "strip %d", t - T_STRIP + 1); return tb; }
            if (t >= T_BTN && t < T_BTN + 128) { const char* s = jam::buttonName(t - T_BTN); return s ? s : "?"; }
            if (t == T_KNOB) return "knob";
            if (t == T_SHIFT) return "SHIFT";
            return "-";
        };
        std::string s = tname(bs[i].normal);
        if (bs[i].shifted != bs[i].normal) s += " / shift: " + tname(bs[i].shifted);
        snprintf(buf, sizeof buf, " = %d", v);
        return s + buf;
    }
    return "";
}

}  // namespace gridctl
