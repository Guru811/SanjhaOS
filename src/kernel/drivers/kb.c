#include <apic.h>
#include <console.h>
#include <kb.h>
#include <paging.h>
#include <task.h>
#include <linux.h>
#include <system.h>

// Full 144-key keyboard driver
// Built by Gurshant Singh (extended from cavOS base)

char characterTable[] = {
    0,    27,   '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',
    '9',  '0',  '-',  '=',  '\b', '\t', 'q',  'w',  'e',  'r',
    't',  'y',  'u',  'i',  'o',  'p',  '[',  ']',  '\n', 0,
    'a',  's',  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',  'b',  'n',
    'm',  ',',  '.',  '/',  0,    '*',  0,    ' ',  0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    '7',  '8',  '9',  '-',  '4',  '5',  '6',  '+',
    '1',  '2',  '3',  '0',  '.',  0,    0,    0,    0,    0,
};

char shiftedCharacterTable[] = {
    0,    27,   '!',  '@',  '#',  '$',  '%',  '^',  '&',  '*',
    '(',  ')',  '_',  '+',  '\b', '\t', 'Q',  'W',  'E',  'R',
    'T',  'Y',  'U',  'I',  'O',  'P',  '{',  '}',  '\n', 0,
    'A',  'S',  'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
    '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',  'B',  'N',
    'M',  '<',  '>',  '?',  0,    '*',  0,    ' ',  0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    '7',  '8',  '9',  '-',  '4',  '5',  '6',  '+',
    '1',  '2',  '3',  '0',  '.',  0,    0,    0,    0,    0,
};

// Numpad with NumLock ON → digits/symbols
// Numpad with NumLock OFF → navigation
char numpadTable[] = {
    // scancode 0x47–0x53
    '7', '8', '9', '-',
    '4', '5', '6', '+',
    '1', '2', '3', '0', '.'
};

const uint8_t evdevTable[89] = {
    0,
    KEY_ESC,
    KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
    KEY_MINUS, KEY_EQUAL, KEY_BACKSPACE, KEY_TAB,
    KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P,
    KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_ENTER, KEY_LEFTCTRL,
    KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L,
    KEY_SEMICOLON, KEY_APOSTROPHE, KEY_GRAVE, KEY_LEFTSHIFT, KEY_BACKSLASH,
    KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M,
    KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_RIGHTSHIFT, KEY_KPASTERISK,
    KEY_LEFTALT, KEY_SPACE, KEY_CAPSLOCK,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
    KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUMLOCK, KEY_SCROLLLOCK,
    KEY_KP7, KEY_UP, KEY_KP9, KEY_KPMINUS,
    KEY_LEFT, KEY_KP5, KEY_RIGHT, KEY_KPPLUS,
    KEY_KP1, KEY_DOWN, KEY_KP3, KEY_INSERT, KEY_DELETE,
    0, 0, 0,
    KEY_F11, KEY_F12,
};

#define EVDEV_INTERNAL_SIZE (DivRoundUp(sizeof(evdevTable) / sizeof(evdevTable[0]), 8))
uint8_t evdevInternal[EVDEV_INTERNAL_SIZE] = {0};

uint8_t lastPressed = 0;
DevInputEvent *kbEvent;

void kbEvdevGenerate(uint8_t raw) {
    uint8_t index   = 0;
    bool    clicked = false;
    if (raw <= 0x58) {
        clicked = true;
        index   = raw;
    } else if (raw <= 0xD8) {
        clicked = false;
        index   = raw - 0x80;
    } else
        return;

    if (index > 88) return;
    uint8_t evdevCode = evdevTable[index];
    if (!evdevCode) return;

    bool oldstate = bitmapGenericGet(evdevInternal, index);
    if (!oldstate && clicked) {
        inputGenerateEvent(kbEvent, EV_KEY, evdevCode, 1);
        lastPressed = evdevCode;
    } else if (oldstate && clicked) {
        if (evdevCode != lastPressed) return;
        inputGenerateEvent(kbEvent, EV_KEY, evdevCode, 2);
    } else if (oldstate && !clicked) {
        inputGenerateEvent(kbEvent, EV_KEY, evdevCode, 0);
    }
    inputGenerateEvent(kbEvent, EV_SYN, SYN_REPORT, 0);
    bitmapGenericSet(evdevInternal, index, clicked);
}

// ── Modifier state ────────────────────────────────────────────────────────────
bool shifted     = false;
bool capsLocked  = false;
bool ctrlHeld    = false;
bool altHeld     = false;
bool numLocked   = true;   // NumLock ON by default
bool scrollLocked = false;
bool extendedKey = false;  // next scancode is 0xE0 extended

// ── KB buffer ─────────────────────────────────────────────────────────────────
char    *kbBuff   = 0;
uint32_t kbCurr   = 0;
uint32_t kbMax    = 0;
uint32_t kbTaskId = 0;

uint8_t kbRead() {
    while (!(inportb(0x64) & 1));
    return inportb(0x60);
}

void kbWrite(uint16_t port, uint8_t value) {
    while (inportb(0x64) & 2);
    outportb(port, value);
}

// ── Special key return codes (internal, >127) ─────────────────────────────────
#define KEY_UP_CODE       0x80
#define KEY_DOWN_CODE     0x81
#define KEY_LEFT_CODE     0x82
#define KEY_RIGHT_CODE    0x83
#define KEY_HOME_CODE     0x84
#define KEY_END_CODE      0x85
#define KEY_DEL_CODE      0x86
#define KEY_PGUP_CODE     0x87
#define KEY_PGDN_CODE     0x88
#define KEY_INS_CODE      0x89
#define KEY_F1_CODE       0x90
#define KEY_F2_CODE       0x91
#define KEY_F3_CODE       0x92
#define KEY_F4_CODE       0x93
#define KEY_F5_CODE       0x94
#define KEY_F6_CODE       0x95
#define KEY_F7_CODE       0x96
#define KEY_F8_CODE       0x97
#define KEY_F9_CODE       0x98
#define KEY_F10_CODE      0x99
#define KEY_F11_CODE      0x9A
#define KEY_F12_CODE      0x9B
#define KEY_KPSLASH_CODE  0x9C  // numpad /  (extended)
#define KEY_KPENTER_CODE  0x9D  // numpad Enter (extended)

char handleKbEvent() {
    uint8_t scanCode = kbRead();

    // Extended key prefix
    if (scanCode == 0xE0) {
        extendedKey = true;
        return 0;
    }

    kbEvdevGenerate(scanCode);

    bool isExtended = extendedKey;
    extendedKey = false;

    // ── Key release ──────────────────────────────────────────────────────────
    if (scanCode & 0x80) {
        uint8_t released = scanCode & 0x7F;
        if (!isExtended) {
            if (released == SCANCODE_SHIFT || released == SCANCODE_SHIFT_RIGHT)
                shifted = false;
            if (released == 29)   // Left Ctrl
                ctrlHeld = false;
            if (released == 56)   // Left Alt
                altHeld = false;
        } else {
            if (released == 29)   // Right Ctrl (E0 1D)
                ctrlHeld = false;
            if (released == 56)   // Right Alt / AltGr (E0 38)
                altHeld = false;
        }
        return 0;
    }

    // ── Extended key press (0xE0 prefix) ─────────────────────────────────────
    if (isExtended) {
        switch (scanCode) {
            // Arrow keys (extended versions — always navigation)
            case 0x48: return KEY_UP_CODE;
            case 0x50: return KEY_DOWN_CODE;
            case 0x4B: return KEY_LEFT_CODE;
            case 0x4D: return KEY_RIGHT_CODE;
            // Navigation cluster
            case 0x47: return KEY_HOME_CODE;
            case 0x4F: return KEY_END_CODE;
            case 0x49: return KEY_PGUP_CODE;
            case 0x51: return KEY_PGDN_CODE;
            case 0x52: return KEY_INS_CODE;
            case 0x53: return KEY_DEL_CODE;
            // Numpad extended
            case 0x35: return KEY_KPSLASH_CODE;  // numpad /
            case 0x1C: return KEY_KPENTER_CODE;  // numpad Enter
            // Right modifiers
            case 0x1D: ctrlHeld = true; return 0; // Right Ctrl
            case 0x38: altHeld  = true; return 0; // Right Alt / AltGr
            default:   return 0;
        }
    }

    // ── Normal key press ─────────────────────────────────────────────────────
    switch (scanCode) {
        // Modifiers
        case SCANCODE_SHIFT:
        case SCANCODE_SHIFT_RIGHT:
            shifted = true;   return 0;
        case 29:  // Left Ctrl
            ctrlHeld = true;  return 0;
        case 56:  // Left Alt
            altHeld  = true;  return 0;
        case SCANCODE_CAPS:
            capsLocked = !capsLocked; return 0;
        case 0x45: // NumLock
            numLocked = !numLocked;   return 0;
        case 0x46: // ScrollLock
            scrollLocked = !scrollLocked; return 0;

        // Common specials
        case SCANCODE_ENTER: return CHARACTER_ENTER;
        case SCANCODE_BACK:  return CHARACTER_BACK;
        case SCANCODE_ESC:   return 27;
        case SCANCODE_TAB:   return '\t';

        // F1–F10 (scancodes 59–68)
        case 59: return KEY_F1_CODE;
        case 60: return KEY_F2_CODE;
        case 61: return KEY_F3_CODE;
        case 62: return KEY_F4_CODE;
        case 63: return KEY_F5_CODE;
        case 64: return KEY_F6_CODE;
        case 65: return KEY_F7_CODE;
        case 66: return KEY_F8_CODE;
        case 67: return KEY_F9_CODE;
        case 68: return KEY_F10_CODE;
        // F11–F12 (scancodes 87–88)
        case 87: return KEY_F11_CODE;
        case 88: return KEY_F12_CODE;

        // Numpad keys (non-extended = numpad)
        // NumLock ON  → digits/symbols
        // NumLock OFF → navigation
        case 0x47: return numLocked ? '7' : KEY_HOME_CODE;
        case 0x48: return numLocked ? '8' : KEY_UP_CODE;
        case 0x49: return numLocked ? '9' : KEY_PGUP_CODE;
        case 0x4A: return '-';                              // numpad -
        case 0x4B: return numLocked ? '4' : KEY_LEFT_CODE;
        case 0x4C: return numLocked ? '5' : 0;             // numpad 5 (no nav)
        case 0x4D: return numLocked ? '6' : KEY_RIGHT_CODE;
        case 0x4E: return '+';                              // numpad +
        case 0x4F: return numLocked ? '1' : KEY_END_CODE;
        case 0x50: return numLocked ? '2' : KEY_DOWN_CODE;
        case 0x51: return numLocked ? '3' : KEY_PGDN_CODE;
        case 0x52: return numLocked ? '0' : KEY_INS_CODE;
        case 0x53: return numLocked ? '.' : KEY_DEL_CODE;
        case 0x37: return '*';                              // numpad *
    }

    // Ctrl combinations
    if (ctrlHeld && scanCode < sizeof(characterTable)) {
        char c = characterTable[scanCode];
        if (c >= 'a' && c <= 'z') return c - 'a' + 1;
        if (c >= 'A' && c <= 'Z') return c - 'A' + 1;
        // Ctrl+[ = ESC, Ctrl+\ etc.
        if (c == '[') return 27;
    }

    // Normal character lookup
    if (scanCode < sizeof(characterTable)) {
        if (capsLocked && !shifted) {
            char c = characterTable[scanCode];
            if (c >= 'a' && c <= 'z')
                return shiftedCharacterTable[scanCode];
            return c;
        }
        char character = (shifted || (capsLocked && shifted))
                             ? shiftedCharacterTable[scanCode]
                             : characterTable[scanCode];
        if (character != 0) return character;
    }

    return 0;
}

// ── Buffer / task helpers ─────────────────────────────────────────────────────

uint32_t readStr(char *buffstr) {
    while (kbIsOccupied());
    bool res = kbTaskRead(KERNEL_TASK_ID, buffstr, 1024, false);
    if (!res) return 0;
    Task *task = taskGet(KERNEL_TASK_ID);
    if (!task) return 0;
    while (kbBuff) {}
    uint32_t ret = task->tmpRecV;
    buffstr[ret] = '\0';
    return ret;
}

bool kbTaskRead(uint32_t taskId, char *buff, uint32_t limit, bool changeTaskState) {
    while (kbIsOccupied());
    Task *task = taskGet(taskId);
    if (!task) return false;
    kbBuff   = buff;
    kbCurr   = 0;
    kbMax    = limit;
    kbTaskId = taskId;
    if (changeTaskState)
        task->state = TASK_STATE_WAITING_INPUT;
    return true;
}

void kbReset() {
    kbBuff   = 0;
    kbCurr   = 0;
    kbMax    = 0;
    kbTaskId = 0;
}

size_t kbEventBit(OpenFile *fd, uint64_t request, void *arg) {
    size_t number = _IOC_NR(request);
    size_t size   = _IOC_SIZE(request);
    size_t ret    = ERR(ENOENT);

    switch (number) {
    case 0x20: {
        size_t out = (1 << EV_SYN) | (1 << EV_KEY);
        ret = MIN(sizeof(size_t), size);
        memcpy(arg, &out, ret);
        break;
    }
    case (0x20 + EV_SW):
    case (0x20 + EV_MSC):
    case (0x20 + EV_SND):
    case (0x20 + EV_LED):
    case (0x20 + EV_REL):
    case (0x20 + EV_ABS):
        ret = MIN(sizeof(size_t), size);
        break;
    case (0x20 + EV_FF):
        ret = MIN(16, size);
        break;
    case (0x20 + EV_KEY): {
        uint8_t map[96] = {0};
        for (int i = KEY_ESC; i <= KEY_MENU; i++)
            bitmapGenericSet(map, i, true);
        ret = MIN(96, size);
        memcpy(arg, map, ret);
        break;
    }
    case 0x18: {
        uint8_t map[96] = {0};
        ret = MIN(96, size);
        memcpy(arg, map, ret);
        break;
    }
    case 0x19: ret = MIN(8, size); break;
    case 0x1b: ret = MIN(8, size); break;
    }
    return ret;
}

void initiateKb() {
    kbEvent = devInputEventSetup("PS/2 Keyboard");
    kbEvent->inputid.bustype = 0x05;
    kbEvent->inputid.vendor  = 0x045e;
    kbEvent->inputid.product = 0x0001;
    kbEvent->inputid.version = 0x0100;
    kbEvent->eventBit        = kbEventBit;

    uint8_t targIrq = ioApicRedirect(1, false);
    registerIRQhandler(targIrq, kbIrq);
    kbReset();
    kbWrite(0x64, 0xae);
    inportb(0x60);
}

void kbFinaliseStream() {
    Task *task = taskGet(kbTaskId);
    if (task) {
        task->tmpRecV = kbCurr;
        task->state   = TASK_STATE_READY;
    }
    kbReset();
}

void kbChar(Task *task, char out) {
    if (task->term.c_lflag & ECHO)
        printfch(out);
    if (kbCurr < kbMax)
        kbBuff[kbCurr++] = out;
    if (!(task->term.c_lflag & ICANON))
        kbFinaliseStream();
}

void kbSendEscape(Task *task, char a, char b, char c) {
    if (kbCurr < kbMax) kbBuff[kbCurr++] = '\033';
    if (kbCurr < kbMax) kbBuff[kbCurr++] = a;
    if (kbCurr < kbMax) kbBuff[kbCurr++] = b;
    if (c && kbCurr < kbMax) kbBuff[kbCurr++] = c;
    if (!(task->term.c_lflag & ICANON))
        kbFinaliseStream();
}

// 4-char escape sequence (e.g. F5 = ESC[15~)
void kbSendEscape4(Task *task, char a, char b, char c, char d) {
    if (kbCurr < kbMax) kbBuff[kbCurr++] = '\033';
    if (kbCurr < kbMax) kbBuff[kbCurr++] = a;
    if (kbCurr < kbMax) kbBuff[kbCurr++] = b;
    if (kbCurr < kbMax) kbBuff[kbCurr++] = c;
    if (d && kbCurr < kbMax) kbBuff[kbCurr++] = d;
    if (!(task->term.c_lflag & ICANON))
        kbFinaliseStream();
}

void kbIrq() {
    char out = handleKbEvent();
    if (!kbBuff || !tasksInitiated) return;

    Task *task = taskGet(kbTaskId);
    if (!task) return;

    switch ((unsigned char)out) {
        // Arrow keys → standard VT escape sequences
        case KEY_UP_CODE:    kbSendEscape(task, '[', 'A', 0);  return;
        case KEY_DOWN_CODE:  kbSendEscape(task, '[', 'B', 0);  return;
        case KEY_RIGHT_CODE: kbSendEscape(task, '[', 'C', 0);  return;
        case KEY_LEFT_CODE:  kbSendEscape(task, '[', 'D', 0);  return;

        // Navigation cluster → VT sequences
        case KEY_HOME_CODE:  kbSendEscape(task, '[', 'H', 0);  return;
        case KEY_END_CODE:   kbSendEscape(task, '[', 'F', 0);  return;
        case KEY_INS_CODE:   kbSendEscape(task, '[', '2', '~'); return;
        case KEY_DEL_CODE:   kbSendEscape(task, '[', '3', '~'); return;
        case KEY_PGUP_CODE:  kbSendEscape(task, '[', '5', '~'); return;
        case KEY_PGDN_CODE:  kbSendEscape(task, '[', '6', '~'); return;

        // Function keys → standard xterm sequences
        case KEY_F1_CODE:  kbSendEscape(task, 'O', 'P', 0);   return;
        case KEY_F2_CODE:  kbSendEscape(task, 'O', 'Q', 0);   return;
        case KEY_F3_CODE:  kbSendEscape(task, 'O', 'R', 0);   return;
        case KEY_F4_CODE:  kbSendEscape(task, 'O', 'S', 0);   return;
        case KEY_F5_CODE:  kbSendEscape4(task, '[', '1', '5', '~'); return;
        case KEY_F6_CODE:  kbSendEscape4(task, '[', '1', '7', '~'); return;
        case KEY_F7_CODE:  kbSendEscape4(task, '[', '1', '8', '~'); return;
        case KEY_F8_CODE:  kbSendEscape4(task, '[', '1', '9', '~'); return;
        case KEY_F9_CODE:  kbSendEscape4(task, '[', '2', '0', '~'); return;
        case KEY_F10_CODE: kbSendEscape4(task, '[', '2', '1', '~'); return;
        case KEY_F11_CODE: kbSendEscape4(task, '[', '2', '3', '~'); return;
        case KEY_F12_CODE: kbSendEscape4(task, '[', '2', '4', '~'); return;

        // Numpad special keys
        case KEY_KPSLASH_CODE: kbChar(task, '/');  return;
        case KEY_KPENTER_CODE: kbChar(task, '\n'); return;
    }

    if (!out) return;

    switch (out) {
    case CHARACTER_ENTER:
        if (task->term.c_lflag & ICANON)
            kbFinaliseStream();
        else
            kbChar(task, '\n');
        break;
    case CHARACTER_BACK:
        if (task->term.c_lflag & ICANON && kbCurr > 0) {
            printfch('\b');
            kbCurr--;
            kbBuff[kbCurr] = 0;
        } else if (!(task->term.c_lflag & ICANON))
            kbChar(task, out);
        break;
    default:
        kbChar(task, out);
        break;
    }
}

bool kbIsOccupied() { return !!kbBuff; }
