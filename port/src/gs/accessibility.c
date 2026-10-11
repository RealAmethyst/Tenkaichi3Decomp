/* Startup speech: native screen events, plus change detection on the text the dialog actually draws.
   The Linux backend uses Prism with Orca on a worker thread. Missing speech support is reported, never fatal to the game. No game state is modified. */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include "../../third_party/prism/prism.h"
#endif
#include "../../include/port_accessibility.h"
#include "accessibility_text.h"

#define TEXT_SIZE 8192
enum SpeechOwner { SPEECH_NONE, SPEECH_SCREEN, SPEECH_DIALOG };
static enum SpeechOwner sOwner;
static int sActive, sDialogOpen, sTitleSaid, sPromptSaid;
static int sTextVisible = -1;
static int sHeadingSaid, sIntroPending, sHadIntro, sChoices, sCursor = -1;
static char sText[2][TEXT_SIZE], sLastBody[TEXT_SIZE];

#ifdef __linux__
static SDL_SharedObject *sLibrary;
static PrismContext *sContext;
static PrismBackend *sBackend;
static PrismConfig (*sConfigInit)(void);
static PrismContext *(*sInit)(PrismConfig *);
static void (*sShutdown)(PrismContext *);
static PrismBackend *(*sCreate)(PrismContext *, PrismBackendId);
static void (*sFree)(PrismBackend *);
static PrismError (*sInitialize)(PrismBackend *);
static PrismError (*sSay)(PrismBackend *, const char *, bool);
static PrismError (*sStop)(PrismBackend *);
static const char *(*sError)(PrismError);
static SDL_Thread *sThread;
static SDL_Mutex *sMutex;
static SDL_Condition *sCondition;
static int sTried, sStopping, sQueued, sStopPending;

struct Utterance {
    struct Utterance *next;
    int interrupt;
    char text[TEXT_SIZE];
};
static struct Utterance *sFirst, *sLast;

static void clear_queue(void) {
    while (sFirst != NULL) {
        struct Utterance *next = sFirst->next;
        free(sFirst);
        sFirst = next;
    }
    sLast = NULL;
    sQueued = 0;
}

static void speech_loop(void) {
    PrismError error;
    for (;;) {
        struct Utterance *utterance;
        SDL_LockMutex(sMutex);
        while (!sStopping && !sStopPending && sFirst == NULL) SDL_WaitCondition(sCondition, sMutex);
        if (sStopping) {
            SDL_UnlockMutex(sMutex);
            break;
        }
        if (sStopPending) {
            sStopPending = 0;
            SDL_UnlockMutex(sMutex);
            error = sStop(sBackend);
            if (error != PRISM_OK) fprintf(stderr, "a11y: Orca stop failed: %s\n", sError(error));
            continue;
        }
        utterance = sFirst;
        sFirst = utterance->next;
        sQueued--;
        if (sFirst == NULL) sLast = NULL;
        SDL_UnlockMutex(sMutex);
        error = sSay(sBackend, utterance->text, utterance->interrupt != 0);
        if (error != PRISM_OK) fprintf(stderr, "a11y: Orca speech failed: %s\n", sError(error));
        free(utterance);
    }
}

static int speech_worker(void *unused) {
    PrismConfig config;
    PrismError error;
    char path[4096];
    const char *base = SDL_GetBasePath();
    (void)unused;
    if (base == NULL || snprintf(path, sizeof(path), "%slib/libprism.so", base) >= (int)sizeof(path)) {
        fprintf(stderr, "a11y: cannot locate Prism beside the executable\n");
        goto stopped;
    }
    sLibrary = SDL_LoadObject(path);
    if (sLibrary == NULL) {
        fprintf(stderr, "a11y: Prism unavailable: %s\n", SDL_GetError());
        goto stopped;
    }
#define LOAD(member, name) member = (void *)SDL_LoadFunction(sLibrary, name)
    LOAD(sConfigInit, "prism_config_init");
    LOAD(sInit, "prism_init");
    LOAD(sShutdown, "prism_shutdown");
    LOAD(sCreate, "prism_registry_create");
    LOAD(sFree, "prism_backend_free");
    LOAD(sInitialize, "prism_backend_initialize");
    LOAD(sSay, "prism_backend_speak");
    LOAD(sStop, "prism_backend_stop");
    LOAD(sError, "prism_error_string");
#undef LOAD
    if (!sConfigInit || !sInit || !sShutdown || !sCreate || !sFree || !sInitialize || !sSay || !sStop || !sError) {
        fprintf(stderr, "a11y: incomplete Prism API: %s\n", SDL_GetError());
        goto stopped;
    }
    config = sConfigInit();
    sContext = sInit(&config);
    if (sContext == NULL) {
        fprintf(stderr, "a11y: Prism initialization failed\n");
        goto stopped;
    }
    sBackend = sCreate(sContext, PRISM_BACKEND_ORCA);
    if (sBackend == NULL) {
        fprintf(stderr, "a11y: Prism Orca backend is missing\n");
        goto stopped;
    }
    error = sInitialize(sBackend);
    if (error != PRISM_OK) {
        fprintf(stderr, "a11y: Orca initialization failed: %s\n", sError(error));
        goto stopped;
    }
    fprintf(stderr, "a11y: Prism 0.16.5, Orca backend ready\n");
    speech_loop();
stopped:
    SDL_LockMutex(sMutex);
    sStopping = 1;
    clear_queue();
    SDL_UnlockMutex(sMutex);
    return 0;
}

static void shutdown_speech(void) {
    if (sThread != NULL) {
        SDL_LockMutex(sMutex);
        sStopping = 1;
        SDL_SignalCondition(sCondition);
        SDL_UnlockMutex(sMutex);
        SDL_WaitThread(sThread, NULL);
    }
    clear_queue();
    if (sBackend != NULL) sFree(sBackend);
    if (sContext != NULL) sShutdown(sContext);
    if (sLibrary != NULL) SDL_UnloadObject(sLibrary);
    SDL_DestroyCondition(sCondition);
    SDL_DestroyMutex(sMutex);
}

static void submit(const char *text, int interrupt) {
    struct Utterance *utterance;
    if (!sTried) {
        sTried = 1;
        sMutex = SDL_CreateMutex();
        sCondition = SDL_CreateCondition();
        if (sMutex != NULL && sCondition != NULL) sThread = SDL_CreateThread(speech_worker, "BT3 speech", NULL);
        atexit(shutdown_speech);
        if (sThread == NULL) fprintf(stderr, "a11y: speech worker unavailable: %s\n", SDL_GetError());
    }
    if (sThread == NULL) return;
    utterance = malloc(sizeof(*utterance));
    if (utterance == NULL) {
        fprintf(stderr, "a11y: could not allocate speech message\n");
        return;
    }
    utterance->next = NULL;
    utterance->interrupt = interrupt;
    snprintf(utterance->text, sizeof(utterance->text), "%s", text);
    SDL_LockMutex(sMutex);
    if (interrupt) clear_queue();
    if (sStopping || sQueued >= 64) {
        if (!sStopping) fprintf(stderr, "a11y: speech queue full; message omitted\n");
        SDL_UnlockMutex(sMutex);
        free(utterance);
        return;
    }
    sQueued++;
    if (sLast != NULL) sLast->next = utterance;
    else sFirst = utterance;
    sLast = utterance;
    SDL_SignalCondition(sCondition);
    SDL_UnlockMutex(sMutex);
}
#endif

/* Shared preparation for artwork and decoded game strings: normalize displayed whitespace.
   Unknown markup is rejected by the UTF-16 decoder instead of speaking invented button names. */
static void speak(const char *text, int interrupt, enum SpeechOwner owner) {
    char clean[TEXT_SIZE];
    size_t n = 0;
    int space = 0;
    const unsigned char *p;

    for (p = (const unsigned char *)text; *p != 0; p++) {
        if (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t') {
            space = n != 0;
            continue;
        }
        if (n + 2 >= sizeof(clean)) {
            fprintf(stderr, "a11y: speech text too long\n");
            return;
        }
        if (space) clean[n++] = ' ';
        clean[n++] = (char)*p;
        space = 0;
    }
    clean[n] = 0;
    if (n == 0) return;
    sOwner = owner;
    fprintf(stderr, "a11y: %s: %s\n", interrupt ? "interrupt" : "queue", clean);
    if (getenv("BT3_SPEECH_LOG_ONLY") != NULL) return;
#ifdef __linux__
    submit(clean, interrupt);
#else
    {
        static int warned;
        if (!warned) fprintf(stderr, "a11y: speech backend not yet available on this platform\n");
        warned = 1;
    }
#endif
}

static void item(const char *name, int cursor, int count, int interrupt, enum SpeechOwner owner) {
    char text[256];
    snprintf(text, sizeof(text), kA11yPosition, name, cursor + 1, count);
    speak(text, interrupt, owner);
}

/* Native screen disappearance cancels both unsubmitted messages and Orca's current speech.
   Keep stop separate from the FIFO so a following screen cannot discard it when interrupting. */
void Port_A11yStop(void) {
    if (sOwner == SPEECH_NONE) return;
    sOwner = SPEECH_NONE;
    fprintf(stderr, "a11y: stop\n");
    if (getenv("BT3_SPEECH_LOG_ONLY") != NULL) return;
#ifdef __linux__
    if (sThread == NULL) return;
    SDL_LockMutex(sMutex);
    clear_queue();
    if (!sStopping) {
        sStopPending = 1;
        SDL_SignalCondition(sCondition);
    }
    SDL_UnlockMutex(sMutex);
#endif
}

void Port_A11yScope(int active) {
    if (!active) Port_A11yStop();
    sActive = active;
    sTitleSaid = sPromptSaid = 0;
    Port_A11yDialogClose();
}

void Port_A11yLogo(int section) {
    if (section >= 3 && section <= 8) speak(kA11yLogo[section], 1, SPEECH_SCREEN);
}

void Port_A11yLegal(void) { speak(kA11yLegal, 1, SPEECH_SCREEN); }

void Port_A11yTitle(void) {
    if (sActive && !sTitleSaid) {
        speak(kA11yTitle, 1, SPEECH_SCREEN);
        sTitleSaid = 1;
    }
}

void Port_A11yTitlePrompt(void) {
    if (sActive && !sPromptSaid) {
        speak(kA11yPressStart, !sTitleSaid, SPEECH_SCREEN);
        sPromptSaid = 1;
    }
}

void Port_A11yTitleItem(int cursor, int count) {
    if (sActive && count >= 1 && count <= 2 && cursor >= 0 && cursor < count) {
        item(kA11yTitleItems[cursor], cursor, count, 1, SPEECH_SCREEN);
    }
}

void Port_A11yDialogOpen(void) {
    if (!sActive || sDialogOpen) return;
    sDialogOpen = 1;
    sHeadingSaid = sIntroPending = sHadIntro = sChoices = 0;
    sCursor = -1;
    sLastBody[0] = 0;
    Port_A11yDialogFrame();
}

void Port_A11yDialogClose(void) {
    if (sDialogOpen && sOwner == SPEECH_DIALOG) Port_A11yStop();
    sDialogOpen = 0;
}

void Port_A11yDialogFrame(void) {
    sText[0][0] = sText[1][0] = 0;
    sTextVisible = -1;
}

void Port_A11yDialogVisibility(int visible) {
    if (sTextVisible < 0) sTextVisible = 0;
    if (visible) sTextVisible = 1;
}

void Port_A11yDialogText(int part, unsigned int address) {
    const uint16_t *p = (const uint16_t *)(uintptr_t)address;
    char *out;
    size_t n = 0;
    unsigned int i, c;

    if (!sActive || !sDialogOpen || part < 0 || part > 1 || p == NULL) return;
    out = sText[part];
    for (i = 0; i < 2048; i++) {
        c = p[i];
        if (c == 0) { out[n] = 0; return; }
        if (c == 0xFEFF) continue;
        if (c == '<') {
            /* FontTag_Color (col_c_b.c): <COL=...> affects appearance, not wording. */
            if (i + 5 >= 2048 || p[i + 1] != 'C' || p[i + 2] != 'O' || p[i + 3] != 'L' || p[i + 4] != '=') {
                fprintf(stderr, "a11y: unsupported game text tag; message omitted\n");
                break;
            }
            while (i < 2048 && p[i] != 0 && p[i] != '>') i++;
            if (i == 2048 || p[i] != '>') break;
            continue;
        }
        if (c >= 0xD800 && c <= 0xDFFF) {
            if (c >= 0xDC00 || i + 1 >= 2048 || p[i + 1] < 0xDC00 || p[i + 1] > 0xDFFF) break;
            c = 0x10000 + ((c - 0xD800) << 10) + (p[++i] - 0xDC00);
        }
        if (n + 4 >= TEXT_SIZE) break;
        if (c < 0x80) out[n++] = (char)c;
        else if (c < 0x800) { out[n++] = (char)(0xC0 | (c >> 6)); out[n++] = (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) {
            out[n++] = (char)(0xE0 | (c >> 12)); out[n++] = (char)(0x80 | ((c >> 6) & 63)); out[n++] = (char)(0x80 | (c & 63));
        } else {
            out[n++] = (char)(0xF0 | (c >> 18)); out[n++] = (char)(0x80 | ((c >> 12) & 63));
            out[n++] = (char)(0x80 | ((c >> 6) & 63)); out[n++] = (char)(0x80 | (c & 63));
        }
    }
    out[0] = 0;
    fprintf(stderr, "a11y: invalid or unterminated game text; message omitted\n");
}

void Port_A11yDialogDraw(int visible, int choices, int cursor) {
    if (!sActive || !sDialogOpen) return;
    if (!visible || sTextVisible == 0) {
        if (sOwner == SPEECH_DIALOG) Port_A11yStop();
        return;
    }
    if (!sHeadingSaid && sText[0][0]) {
        speak(sText[0], 1, SPEECH_DIALOG);
        sHeadingSaid = sIntroPending = sHadIntro = 1;
    }
    if (sText[1][0] && strcmp(sLastBody, sText[1]) != 0) {
        speak(sText[1], !sIntroPending, SPEECH_DIALOG);
        snprintf(sLastBody, sizeof(sLastBody), "%s", sText[1]);
        sIntroPending = 0;
        sHadIntro = 1;
        sChoices = 0; /* the message changed; announce its current choice after it */
    }
    if (choices && sText[1][0] && cursor >= 0 && cursor < 2 && (!sChoices || cursor != sCursor)) {
        item(kA11yDialogItems[cursor], cursor, 2, sChoices || !sHadIntro, SPEECH_DIALOG);
        sCursor = cursor;
        sChoices = 1;
    }
    if (!choices) sChoices = 0;
}
