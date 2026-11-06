#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

#define W 400
#define MAXI 8
#define PI 3.14159265f
#define SR 32000
#define C(r,g,b) C2D_Color32(r,g,b,255)
#define CA(r,g,b,a) C2D_Color32(r,g,b,a)

enum { NONE, LUPE, BIER, HANDS, ZIGA, SAEGE };
static const char *iname[] = {"", "Lupe", "Bier", "Fessel", "Zigarette", "Säge"};

/* ================= TON ================= */
enum { S_SHOT, S_CLICK, S_COCK, S_PING, S_BEER, S_CUFF, S_HEAL, S_SAW, S_TICK, S_WIN, S_LOSE, S_ERR, S_DRONE, NSND };
static const float sndLen[NSND] = {0.7f, 0.12f, 0.35f, 0.5f, 0.6f, 0.35f, 0.5f, 0.6f, 0.05f, 0.8f, 0.9f, 0.15f, 2.0f};
typedef struct { s16 *buf; int n; ndspWaveBuf wb; } Snd;
static Snd snd[NSND];
static int audioOK;

static float rnd(void) { return rand() / (float)RAND_MAX * 2.0f - 1.0f; }

static float sampleOf(int id, float t, float *lp) {
    float n = rnd(), v = 0;
    switch (id) {
        case S_SHOT:
            *lp = *lp * 0.7f + n * 0.3f;
            v = *lp * 2.2f * expf(-t * 8) + sinf(2 * PI * 55 * t) * expf(-t * 6) + 0.4f * sinf(2 * PI * 110 * t) * expf(-t * 12);
            break;
        case S_CLICK:
            v = n * expf(-t * 90) * 0.5f + sinf(2 * PI * 1800 * t) * expf(-t * 70) * 0.4f;
            break;
        case S_COCK: {
            float b = t - 0.14f;
            v = expf(-t * 70) * (n * 0.6f + sinf(2 * PI * 900 * t) * 0.4f);
            if (b > 0) v += expf(-b * 70) * (n * 0.6f + sinf(2 * PI * 700 * b) * 0.4f);
            break;
        }
        case S_PING:
            v = sinf(2 * PI * 1760 * t) * expf(-t * 6) * 0.5f;
            break;
        case S_BEER:
            *lp = *lp * 0.85f + n * 0.15f;
            v = *lp * (1.5f + sinf(t * 60) * 0.5f) * expf(-t * 3.5f) + sinf(2 * PI * 200 * t) * expf(-t * 40) * 0.5f;
            break;
        case S_CUFF: {
            float b = t - 0.09f;
            v = (sinf(2 * PI * 2200 * t) + 0.6f * sinf(2 * PI * 3300 * t)) * expf(-t * 40) * 0.3f;
            if (b > 0) v += (sinf(2 * PI * 2000 * b) + 0.6f * sinf(2 * PI * 3100 * b)) * expf(-b * 40) * 0.3f;
            break;
        }
        case S_HEAL:
            v = sinf(2 * PI * (440 * t + 550 * t * t)) * 0.4f * expf(-t * 2.5f);
            break;
        case S_SAW: {
            float env = t < 0.5f ? 1.0f : expf(-(t - 0.5f) * 20);
            v = (fmodf(t * 120.0f, 1.0f) * 2 - 1) * 0.35f * env + n * 0.25f * env;
            break;
        }
        case S_TICK:
            v = sinf(2 * PI * 1200 * t) * expf(-t * 80) * 0.5f;
            break;
        case S_WIN: {
            static const float nt[4] = {523.25f, 659.25f, 783.99f, 1046.5f};
            int k = (int)(t / 0.15f);
            if (k > 3) k = 3;
            float a = t - k * 0.15f;
            v = sinf(2 * PI * nt[k] * a) * expf(-a * 6) * 0.5f;
            break;
        }
        case S_LOSE: {
            static const float nt[3] = {196.0f, 164.8f, 130.8f};
            int k = (int)(t / 0.28f);
            if (k > 2) k = 2;
            float a = t - k * 0.28f;
            v = sinf(2 * PI * nt[k] * a) * expf(-a * 3) * 0.5f + n * 0.05f * expf(-t * 2);
            break;
        }
        case S_ERR:
            v = (sinf(2 * PI * 150 * t) > 0 ? 1.0f : -1.0f) * 0.25f * expf(-t * 15);
            break;
        case S_DRONE:
            v = (sinf(2 * PI * 55 * t) * 0.6f + sinf(2 * PI * 82.5f * t) * 0.4f) * (0.6f + 0.4f * sinf(2 * PI * 0.5f * t));
            break;
    }
    return v;
}

static void initAudio(void) {
    if (R_FAILED(ndspInit())) return;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    for (int id = 0; id < NSND; id++) {
        int n = (int)(sndLen[id] * SR);
        s16 *b = (s16 *)linearAlloc(n * sizeof(s16));
        if (!b) return;
        float lp = 0;
        for (int i = 0; i < n; i++) {
            float v = sampleOf(id, i / (float)SR, &lp);
            if (v > 1) v = 1;
            if (v < -1) v = -1;
            b[i] = (s16)(v * 30000);
        }
        DSP_FlushDataCache(b, n * sizeof(s16));
        snd[id].buf = b;
        snd[id].n = n;
    }
    audioOK = 1;
}

static void play(int id, float vol) {
    if (!audioOK) return;
    Snd *s = &snd[id];
    float mix[12] = {0};
    mix[0] = vol;
    mix[1] = vol;
    ndspChanReset(id);
    ndspChanSetInterp(id, NDSP_INTERP_LINEAR);
    ndspChanSetRate(id, (float)SR);
    ndspChanSetFormat(id, NDSP_FORMAT_MONO_PCM16);
    ndspChanSetMix(id, mix);
    memset(&s->wb, 0, sizeof(ndspWaveBuf));
    s->wb.data_vaddr = s->buf;
    s->wb.nsamples = s->n;
    s->wb.looping = (id == S_DRONE);
    DSP_FlushDataCache(s->buf, s->n * sizeof(s16));
    ndspChanWaveBufAdd(id, &s->wb);
}

/* ================= SPIELLOGIK ================= */
typedef struct {
    int hp, maxhp, cuffed, saw;
    int items[MAXI];
} Fighter;

static Fighter pl, dl;
static int shells[8], nsh, pos, liveLeft, blankLeft, loadLive, loadBlank;
static int peek[2];
static int round_, turn, state, dealT, pendingNext, needReload;
static int banner;
static char bannerTitle[32];
static int animState, animT, shotShooter, shotSelf, fireLive, fireDmg, fireHitPlayer;
static char msgs[2][56];
static unsigned frame;

static const char *name(int who) { return who ? "Dealer" : "Du"; }

static void addlog(const char *f, ...) {
    strcpy(msgs[0], msgs[1]);
    va_list a;
    va_start(a, f);
    vsnprintf(msgs[1], sizeof(msgs[1]), f, a);
    va_end(a);
}

static void giveItems(Fighter *f, int n) {
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < MAXI; i++) {
            if (f->items[i] == NONE) {
                f->items[i] = 1 + rand() % 5;
                break;
            }
        }
    }
}

static void load(const char *title) {
    nsh = 2 + rand() % 7;
    int live = 1 + rand() % (nsh - 1);
    for (int i = 0; i < nsh; i++) shells[i] = (i < live);
    for (int i = nsh - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = shells[i];
        shells[i] = shells[j];
        shells[j] = t;
    }
    pos = 0;
    liveLeft = loadLive = live;
    blankLeft = loadBlank = nsh - live;
    peek[0] = peek[1] = 0;
    if (round_ > 1) {
        giveItems(&pl, round_);
        giveItems(&dl, round_);
    }
    snprintf(bannerTitle, sizeof(bannerTitle), "%s", title);
    banner = 170;
    play(S_COCK, 0.8f);
}

static void startRound(void) {
    char t[32];
    int mh = 2 + round_;
    memset(&pl, 0, sizeof(pl));
    memset(&dl, 0, sizeof(dl));
    pl.hp = pl.maxhp = mh;
    dl.hp = dl.maxhp = mh;
    turn = 0;
    dealT = 40;
    snprintf(t, sizeof(t), "RUNDE %d", round_);
    load(t);
}

static void newGame(void) {
    memset(msgs, 0, sizeof(msgs));
    round_ = 1;
    state = 0;
    animState = 0;
    pendingNext = needReload = 0;
    addlog("Viel Glück...");
    startRound();
}

static int useItem(int who, int item) {
    Fighter *f = who ? &dl : &pl;
    Fighter *o = who ? &pl : &dl;
    int slot = -1;
    for (int i = 0; i < MAXI; i++) {
        if (f->items[i] == item) { slot = i; break; }
    }
    if (slot < 0) return 0;
    switch (item) {
        case LUPE:
            peek[who] = shells[pos] ? 1 : 2;
            if (who == 0) addlog("Lupe: Patrone ist %s", shells[pos] ? "SCHARF" : "BLIND");
            else addlog("Dealer benutzt die Lupe");
            play(S_PING, 0.7f);
            break;
        case BIER: {
            int l = shells[pos++];
            if (l) liveLeft--; else blankLeft--;
            peek[0] = peek[1] = 0;
            addlog("%s: Bier, %s ausgeworfen", name(who), l ? "SCHARF" : "BLIND");
            play(S_BEER, 0.7f);
            break;
        }
        case HANDS:
            if (o->cuffed) return 0;
            o->cuffed = 1;
            addlog("%s: Fessel angelegt", name(who));
            play(S_CUFF, 0.7f);
            break;
        case ZIGA:
            if (f->hp >= f->maxhp) return 0;
            f->hp++;
            addlog("%s raucht: +1 Leben", name(who));
            play(S_HEAL, 0.7f);
            break;
        case SAEGE:
            if (f->saw) return 0;
            f->saw = 1;
            addlog("%s: Lauf abgesägt (x2)", name(who));
            play(S_SAW, 0.7f);
            break;
        default:
            return 0;
    }
    f->items[slot] = NONE;
    if (pos >= nsh) load("NEU GELADEN");
    return 1;
}

static void startShot(int shooter, int self) {
    if (animState || banner || state) return;
    shotShooter = shooter;
    shotSelf = self;
    animState = 1;
    animT = 0;
    play(S_COCK, 0.7f);
}

static void resolveShot(void) {
    int shooter = shotShooter, self = shotSelf;
    Fighter *s = shooter ? &dl : &pl;
    Fighter *o = shooter ? &pl : &dl;
    Fighter *t = self ? s : o;
    int live = shells[pos++];
    if (live) liveLeft--; else blankLeft--;
    int dmg = live ? (s->saw ? 2 : 1) : 0;
    s->saw = 0;
    peek[0] = peek[1] = 0;
    t->hp -= dmg;
    fireLive = live;
    fireDmg = dmg;
    fireHitPlayer = (t == &pl);
    play(live ? S_SHOT : S_CLICK, live ? 1.0f : 0.8f);
    addlog("%s -> %s: %s%s", name(shooter), self ? "sich selbst" : name(1 - shooter),
           live ? "SCHARF" : "BLIND", dmg == 2 ? " (x2)" : "");

    if (pl.hp <= 0) { state = 2; play(S_LOSE, 0.9f); return; }
    if (dl.hp <= 0) {
        if (round_ >= 3) { state = 1; play(S_WIN, 0.9f); }
        else pendingNext = 1;
        return;
    }
    int next = shooter;
    if (!(self && !live)) {
        int other = 1 - shooter;
        if (o->cuffed) {
            o->cuffed = 0;
            addlog("%s setzt aus (Fessel)", name(other));
            play(S_CUFF, 0.6f);
        } else {
            next = other;
        }
    }
    turn = next;
    if (pos >= nsh) needReload = 1;
}

static void dealerAct(void) {
    if (dl.hp < dl.maxhp && useItem(1, ZIGA)) return;
    if (!peek[1] && useItem(1, LUPE)) return;
    if (!pl.cuffed && useItem(1, HANDS)) return;
    if (!peek[1] && rand() % 2 && useItem(1, BIER)) return;
    if (!dl.saw && (peek[1] == 1 || (peek[1] == 0 && liveLeft > blankLeft)) && useItem(1, SAEGE)) return;
    int self = 0;
    if (peek[1] == 2) self = 1;
    else if (peek[1] == 0 && blankLeft > liveLeft) self = 1;
    startShot(1, self);
}

/* ================= GRAFIK ================= */
static C2D_TextBuf tbuf;

static void txt(const char *s, float x, float y, float sc, u32 clr, int align) {
    C2D_Text t;
    float w, h;
    C2D_TextParse(&t, tbuf, s);
    C2D_TextOptimize(&t);
    C2D_TextGetDimensions(&t, sc, sc, &w, &h);
    if (align == 1) x -= w / 2;
    else if (align == 2) x -= w;
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.0f, sc, sc, clr);
}

static void txtS(const char *s, float x, float y, float sc, u32 clr, int align) {
    txt(s, x + 1.5f, y + 1.5f, sc, C(0, 0, 0), align);
    txt(s, x, y, sc, clr, align);
}

static void heart(float x, float y, float s, u32 c) {
    C2D_DrawCircleSolid(x + s * 0.28f, y + s * 0.3f, 0, s * 0.28f, c);
    C2D_DrawCircleSolid(x + s * 0.72f, y + s * 0.3f, 0, s * 0.28f, c);
    C2D_DrawTriangle(x + s * 0.02f, y + s * 0.38f, c, x + s * 0.98f, y + s * 0.38f, c, x + s * 0.5f, y + s * 0.95f, c, 0);
}

static void shellPip(float x, float y, float w, float h, int live) {
    u32 c = live ? C(200, 40, 40) : C(60, 110, 200);
    C2D_DrawRectSolid(x, y, 0, w, h, c);
    C2D_DrawRectSolid(x, y + h - h * 0.25f, 0, w, h * 0.25f, C(200, 160, 60));
}

#define R_(x,y,w,h,c) C2D_DrawRectSolid(x,y,0,w,h,c)
#define CI_(x,y,r,c) C2D_DrawCircleSolid(x,y,0,r,c)

static void icon(int item, float x, float y, float s) {
    u32 st = C(190, 190, 200);
    switch (item) {
        case LUPE:
            C2D_DrawLine(x + 20 * s, y + 20 * s, C(110, 70, 30), x + 30 * s, y + 30 * s, C(110, 70, 30), 4 * s, 0);
            CI_(x + 13 * s, y + 13 * s, 11 * s, C(160, 200, 230));
            CI_(x + 13 * s, y + 13 * s, 8 * s, C(70, 100, 130));
            break;
        case BIER:
            R_(x + 9 * s, y + 6 * s, 14 * s, 22 * s, C(205, 170, 40));
            R_(x + 9 * s, y + 6 * s, 14 * s, 4 * s, C(225, 225, 225));
            R_(x + 9 * s, y + 14 * s, 14 * s, 6 * s, C(180, 40, 40));
            break;
        case HANDS:
            C2D_DrawLine(x + 9 * s, y + 20 * s, st, x + 23 * s, y + 20 * s, st, 3 * s, 0);
            CI_(x + 9 * s, y + 20 * s, 8 * s, st);
            CI_(x + 9 * s, y + 20 * s, 4 * s, C(40, 40, 50));
            CI_(x + 23 * s, y + 20 * s, 8 * s, st);
            CI_(x + 23 * s, y + 20 * s, 4 * s, C(40, 40, 50));
            break;
        case ZIGA:
            R_(x + 3 * s, y + 15 * s, 22 * s, 5 * s, C(235, 235, 225));
            R_(x + 3 * s, y + 15 * s, 7 * s, 5 * s, C(220, 140, 50));
            R_(x + 25 * s, y + 15 * s, 3 * s, 5 * s, C(230, 60, 30));
            C2D_DrawLine(x + 28 * s, y + 13 * s, C(150, 150, 150), x + 25 * s, y + 4 * s, C(150, 150, 150), 2 * s, 0);
            break;
        case SAEGE:
            R_(x + 4 * s, y + 12 * s, 22 * s, 6 * s, st);
            for (int i = 0; i < 5; i++)
                C2D_DrawTriangle(x + (4 + i * 4.4f) * s, y + 18 * s, st, x + (6.2f + i * 4.4f) * s, y + 24 * s, st,
                                 x + (8.4f + i * 4.4f) * s, y + 18 * s, st, 0);
            R_(x + 26 * s, y + 10 * s, 6 * s, 9 * s, C(110, 60, 30));
            break;
    }
}

static void drawDealer(float cx, float cy, int hurt) {
    u32 skin = hurt ? C(235, 95, 85) : C(206, 200, 184);
    u32 dark = C(10, 10, 12);
    u32 red = C(170, 30, 30);
    float look = sinf(frame * 0.03f) * 3;
    C2D_DrawRectSolid(cx - 62, cy + 38, 0, 124, 80, C(28, 28, 34));
    C2D_DrawTriangle(cx - 20, cy + 36, C(235, 235, 235), cx + 20, cy + 36, C(235, 235, 235), cx, cy + 96, C(235, 235, 235), 0);
    C2D_DrawRectSolid(cx - 10, cy + 28, 0, 20, 14, skin);
    C2D_DrawTriangle(cx, cy + 46, red, cx - 15, cy + 37, red, cx - 15, cy + 55, red, 0);
    C2D_DrawTriangle(cx, cy + 46, red, cx + 15, cy + 37, red, cx + 15, cy + 55, red, 0);
    C2D_DrawEllipseSolid(cx - 34, cy - 44, 0, 68, 78, skin);
    C2D_DrawEllipseSolid(cx - 26, cy - 24, 0, 20, 24, dark);
    C2D_DrawEllipseSolid(cx + 6, cy - 24, 0, 20, 24, dark);
    C2D_DrawCircleSolid(cx - 16 + look, cy - 12, 0, 3.5f, C(240, 240, 240));
    C2D_DrawCircleSolid(cx + 16 + look, cy - 12, 0, 3.5f, C(240, 240, 240));
    C2D_DrawLine(cx - 30, cy - 32, dark, cx - 8, cy - 26, dark, 3, 0);
    C2D_DrawLine(cx + 30, cy - 32, dark, cx + 8, cy - 26, dark, 3, 0);
    C2D_DrawRectSolid(cx - 26, cy + 8, 0, 52, 15, dark);
    for (int i = 0; i < 9; i++) {
        C2D_DrawRectSolid(cx - 24 + i * 5.6f, cy + 8, 0, 4, 6, C(235, 235, 225));
        if (i < 8) C2D_DrawRectSolid(cx - 21 + i * 5.6f, cy + 17, 0, 4, 5, C(235, 235, 225));
    }
    C2D_DrawLine(cx - 26, cy + 10, dark, cx - 33, cy + 1, dark, 2, 0);
    C2D_DrawLine(cx + 26, cy + 10, dark, cx + 33, cy + 1, dark, 2, 0);
}

static int aimAtDealer(void) { return shotSelf ? (shotShooter == 1) : (shotShooter == 0); }

static void drawTop(void) {
    float shx = 0, shy = 0;
    if (animState == 2 && fireLive) {
        float k = (40 - animT) / 40.0f;
        shx = sinf(animT * 2.1f) * 8 * k;
        shy = cosf(animT * 1.7f) * 8 * k;
    }
    C2D_ViewReset();
    C2D_ViewTranslate(shx, shy);

    /* Wand + Licht */
    C2D_DrawRectangle(-20, -20, 0, W + 40, 190, C(16, 12, 14), C(16, 12, 14), C(34, 24, 26), C(34, 24, 26));
    C2D_DrawCircleSolid(200, 100, 0, 120, CA(255, 235, 190, 12));
    C2D_DrawCircleSolid(200, 100, 0, 85, CA(255, 235, 190, 14));
    C2D_DrawCircleSolid(200, 100, 0, 55, CA(255, 235, 190, 16));

    /* Dealer */
    int hurt = (animState == 2 && !fireHitPlayer && fireLive && fireDmg > 0 && animT < 14);
    drawDealer(200 + (hurt ? sinf(animT * 3.0f) * 3 : 0), 88 + sinf(frame * 0.04f) * 2, hurt);
    if (dl.cuffed) icon(HANDS, 232, 52, 1.0f);

    /* Tisch */
    C2D_DrawRectangle(-20, 150, 0, W + 40, 110, C(96, 60, 34), C(96, 60, 34), C(44, 26, 14), C(44, 26, 14));
    C2D_DrawRectSolid(-20, 150, 0, W + 40, 3, C(150, 100, 58));

    /* Dealer-Items */
    int k = 0;
    for (int i = 0; i < MAXI; i++) {
        if (dl.items[i]) {
            icon(dl.items[i], 96 + k * 30, 155, 0.8f);
            k++;
        }
    }

    /* Schrotflinte */
    float idle = -PI / 2 + sinf(frame * 0.03f) * 0.05f;
    float ang = idle;
    float recoil = 0;
    if (animState == 1) {
        float tgt = aimAtDealer() ? -PI / 2 : PI / 2;
        float kk = animT / 28.0f;
        kk = kk * kk * (3 - 2 * kk);
        ang = idle + (tgt - idle) * kk;
    } else if (animState == 2) {
        ang = aimAtDealer() ? -PI / 2 : PI / 2;
        if (fireLive && animT < 10) recoil = 14.0f * (1 - animT / 10.0f);
    }
    float ca = cosf(ang), sa = sinf(ang);
    float cx = 200 - ca * recoil, cy = 218 - sa * recoil;
    float mx = cx + ca * 85, my = cy + sa * 85;
    u32 wood = C(120, 70, 36), wood2 = C(90, 50, 26), steel = C(120, 124, 132);
    C2D_DrawLine(cx - ca * 48, cy - sa * 48, wood, cx, cy, wood, 13, 0);
    C2D_DrawLine(cx, cy, steel, mx, my, steel, 7, 0);
    C2D_DrawLine(cx + ca * 28, cy + sa * 28, wood2, cx + ca * 60, cy + sa * 60, wood2, 11, 0);
    if (my > 226) my = 226;

    /* Leben */
    txtS("DEALER", 10, 6, 0.45f, C(230, 230, 230), 0);
    for (int i = 0; i < dl.maxhp; i++) heart(10 + i * 19, 24, 17, i < dl.hp ? C(220, 40, 60) : C(70, 40, 46));
    if (dl.cuffed) txtS("[Fessel]", 10, 44, 0.4f, C(200, 200, 220), 0);
    if (dl.saw) txtS("[Säge]", 62, 44, 0.4f, C(230, 180, 90), 0);
    txtS("DU", 10, 160, 0.45f, C(230, 230, 230), 0);
    for (int i = 0; i < pl.maxhp; i++) heart(10 + i * 19, 178, 17, i < pl.hp ? C(220, 40, 60) : C(70, 40, 46));
    if (pl.cuffed) txtS("[Fessel]", 10, 198, 0.4f, C(200, 200, 220), 0);
    if (pl.saw) txtS("[Säge]", 62, 198, 0.4f, C(230, 180, 90), 0);

    /* Runde + Patronen */
    char buf[48];
    snprintf(buf, sizeof(buf), "Runde %d/3", round_);
    txtS(buf, W - 10, 6, 0.45f, C(230, 230, 230), 2);
    txtS("Übrig:", W - 10, 24, 0.4f, C(200, 200, 200), 2);
    int tot = liveLeft + blankLeft;
    for (int i = 0; i < tot; i++) shellPip(W - 14 - i * 13, 42, 9, 20, i < liveLeft);

    /* Effekte */
    if (animState == 2) {
        if (fireLive && animT < 14) {
            float r = 44.0f * (1 - animT / 14.0f) + 8;
            C2D_DrawCircleSolid(mx, my, 0, r, CA(255, 120, 20, 230));
            C2D_DrawCircleSolid(mx, my, 0, r * 0.7f, CA(255, 220, 60, 240));
            C2D_DrawCircleSolid(mx, my, 0, r * 0.4f, C(255, 255, 255));
        }
        if (!fireLive && animT < 24) txtS("*klick*", mx, my - 30 - animT, 0.6f, C(210, 210, 210), 1);
        if (fireLive && animT < 6) C2D_DrawRectSolid(-20, -20, 0, W + 40, 280, CA(255, 255, 230, (int)(200 * (1 - animT / 6.0f))));
        if (fireLive && fireHitPlayer && fireDmg > 0 && animT < 30)
            C2D_DrawRectSolid(-20, -20, 0, W + 40, 280, CA(200, 0, 0, (int)(130 * (1 - animT / 30.0f))));
    }

    C2D_ViewReset();

    /* Log */
    C2D_DrawRectSolid(0, 212, 0, W, 28, CA(0, 0, 0, 180));
    txt(msgs[0], 8, 212, 0.4f, C(150, 150, 150), 0);
    txt(msgs[1], 8, 225, 0.4f, C(240, 240, 240), 0);

    /* Banner (Nachladen) */
    if (banner > 0) {
        C2D_DrawRectSolid(30, 40, 0, 340, 150, CA(0, 0, 0, 225));
        C2D_DrawRectSolid(30, 40, 0, 340, 3, C(170, 30, 30));
        C2D_DrawRectSolid(30, 187, 0, 340, 3, C(170, 30, 30));
        txtS(bannerTitle, 200, 52, 0.8f, C(240, 240, 240), 1);
        snprintf(buf, sizeof(buf), "%d scharf   %d blind", loadLive, loadBlank);
        txtS(buf, 200, 92, 0.55f, C(230, 200, 120), 1);
        int tn = loadLive + loadBlank;
        float x0 = 200 - tn * 11.0f;
        for (int i = 0; i < tn; i++) shellPip(x0 + i * 22, 125, 16, 40, i < loadLive);
    }

    /* Spielende */
    if (state != 0 && animState == 0) {
        C2D_DrawRectSolid(0, 0, 0, W, 240, CA(0, 0, 0, 200));
        if (state == 1) txtS("DU HAST GEWONNEN!", 200, 90, 0.9f, C(120, 230, 120), 1);
        else txtS("GAME OVER", 200, 90, 1.0f, C(230, 60, 60), 1);
    }
}

typedef struct { int x, y, w, h; } Rect;
static const Rect BTN_D = {10, 22, 148, 62};
static const Rect BTN_S = {162, 22, 148, 62};

static Rect slotRect(int i) {
    Rect r = {6 + (i % 4) * 78, 94 + (i / 4) * 72, 74, 66};
    return r;
}

static int inRect(Rect r, int px, int py) { return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h; }

static void button(Rect r, const char *label, u32 col, int pressed) {
    u32 c = pressed ? col + 0x00303030 : col;
    C2D_DrawRectSolid(r.x, r.y, 0, r.w, r.h, C(10, 10, 10));
    C2D_DrawRectSolid(r.x + 3, r.y + 3, 0, r.w - 6, r.h - 6, c);
    txtS(label, r.x + r.w / 2.0f, r.y + r.h / 2.0f - 10, 0.6f, C(255, 255, 255), 1);
}

static void drawBottom(int touching, int tx, int ty) {
    C2D_ViewReset();
    C2D_DrawRectSolid(0, 0, 0, 320, 240, C(14, 14, 20));
    if (state != 0) {
        txtS(state == 1 ? "GEWONNEN!" : "VERLOREN", 160, 60, 0.9f, C(240, 240, 240), 1);
        button((Rect){60, 120, 200, 70}, "NOCHMAL", C(150, 30, 30), touching);
        return;
    }
    int mine = (turn == 0 && !animState && !banner);
    txtS(turn == 0 ? "DEIN ZUG" : "DEALER ÜBERLEGT...", 160, 2, 0.5f, C(230, 230, 230), 1);
    u32 gray = C(60, 60, 66);
    button(BTN_D, "DEALER", mine ? C(150, 30, 30) : gray, mine && touching && inRect(BTN_D, tx, ty));
    button(BTN_S, "SELBST", mine ? C(40, 70, 130) : gray, mine && touching && inRect(BTN_S, tx, ty));
    txtS("Items", 160, 76, 0.4f, C(170, 170, 170), 1);
    for (int i = 0; i < MAXI; i++) {
        Rect r = slotRect(i);
        int has = pl.items[i] != NONE;
        C2D_DrawRectSolid(r.x, r.y, 0, r.w, r.h, C(8, 8, 8));
        C2D_DrawRectSolid(r.x + 2, r.y + 2, 0, r.w - 4, r.h - 4,
                          has ? (touching && inRect(r, tx, ty) ? C(80, 80, 96) : C(44, 44, 56)) : C(24, 24, 30));
        if (has) {
            icon(pl.items[i], r.x + r.w / 2.0f - 20, r.y + 6, 1.25f);
            txt(iname[pl.items[i]], r.x + r.w / 2.0f, r.y + 47, 0.4f, C(230, 230, 230), 1);
        }
    }
}

/* ================= MAIN ================= */
int main(void) {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    tbuf = C2D_TextBufNew(4096);
    srand((unsigned)time(NULL));
    initAudio();
    play(S_DRONE, 0.12f);
    newGame();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kd = hidKeysDown(), kh = hidKeysHeld();
        if (kd & KEY_START) break;
        touchPosition tp;
        hidTouchRead(&tp);
        int tx = tp.px, ty = tp.py;
        int touching = (kh & KEY_TOUCH) != 0;
        int tapped = (kd & KEY_TOUCH) != 0;
        frame++;

        if (banner > 0) {
            banner--;
            if (banner == 0) dealT = 30;
        } else if (animState == 1) {
            animT++;
            if (animT >= 28) { resolveShot(); animState = 2; animT = 0; }
        } else if (animState == 2) {
            animT++;
            if (animT >= 40) {
                animState = 0;
                dealT = 40;
                if (pendingNext) { pendingNext = 0; round_++; startRound(); }
                else if (needReload) { needReload = 0; load("NEU GELADEN"); }
            }
        } else if (state == 0) {
            if (turn == 0) {
                if (kd & KEY_A) startShot(0, 0);
                else if (kd & KEY_B) startShot(0, 1);
                else if (tapped) {
                    if (inRect(BTN_D, tx, ty)) startShot(0, 0);
                    else if (inRect(BTN_S, tx, ty)) startShot(0, 1);
                    else {
                        for (int i = 0; i < MAXI; i++) {
                            if (inRect(slotRect(i), tx, ty) && pl.items[i]) {
                                if (!useItem(0, pl.items[i])) { addlog("Das geht gerade nicht"); play(S_ERR, 0.6f); }
                                break;
                            }
                        }
                    }
                }
            } else {
                if (dealT > 0) dealT--;
                else { dealerAct(); dealT = 55; }
            }
        } else {
            if ((kd & KEY_A) || tapped) newGame();
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(tbuf);
        C2D_TargetClear(top, C(0, 0, 0));
        C2D_SceneBegin(top);
        drawTop();
        C2D_TargetClear(bot, C(0, 0, 0));
        C2D_SceneBegin(bot);
        drawBottom(touching, tx, ty);
        C3D_FrameEnd(0);
    }

    if (audioOK) ndspExit();
    C2D_TextBufDelete(tbuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
