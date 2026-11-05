#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#define MAXI 8
#define LOGN 9

enum { NONE, LUPE, BIER, HANDS, ZIGA, SAEGE };
static const char *iname[] = {"-", "Lupe", "Bier", "Handschellen", "Zigarette", "Saege"};

typedef struct {
    int hp, maxhp, cuffed, saw;
    int items[MAXI];
} Fighter;

static Fighter pl, dl;
static int shells[8], nsh, pos, liveLeft, blankLeft;
static int peek[2];
static int round_, turn, state, sel, timer, dirty = 1;
static char logb[LOGN][52];
static PrintConsole top, bot;

static const char *name(int who) { return who ? "Dealer" : "Du"; }

static void addlog(const char *f, ...) {
    for (int i = 0; i < LOGN - 1; i++) strcpy(logb[i], logb[i + 1]);
    va_list a;
    va_start(a, f);
    vsnprintf(logb[LOGN - 1], sizeof(logb[0]), f, a);
    va_end(a);
    dirty = 1;
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

static void load(void) {
    nsh = 2 + rand() % 7;
    int live = 1 + rand() % (nsh - 1);
    for (int i = 0; i < nsh; i++) shells[i] = (i < live);
    for (int i = nsh - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = shells[i]; shells[i] = shells[j]; shells[j] = t;
    }
    pos = 0;
    liveLeft = live;
    blankLeft = nsh - live;
    peek[0] = peek[1] = 0;
    addlog("Neu geladen: %d scharf, %d blind", liveLeft, blankLeft);
    if (round_ > 1) {
        giveItems(&pl, round_);
        giveItems(&dl, round_);
        addlog("Neue Items verteilt");
    }
}

static void startRound(void) {
    int mh = 2 + round_;
    memset(&pl, 0, sizeof(pl));
    memset(&dl, 0, sizeof(dl));
    pl.hp = pl.maxhp = mh;
    dl.hp = dl.maxhp = mh;
    turn = 0;
    sel = 0;
    timer = 0;
    addlog("--- Runde %d ---", round_);
    load();
}

static void newGame(void) {
    memset(logb, 0, sizeof(logb));
    round_ = 1;
    state = 0;
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
            break;
        case BIER: {
            int l = shells[pos++];
            if (l) liveLeft--; else blankLeft--;
            peek[0] = peek[1] = 0;
            addlog("%s: Bier, %s ausgeworfen", name(who), l ? "SCHARF" : "BLIND");
            break;
        }
        case HANDS:
            if (o->cuffed) return 0;
            o->cuffed = 1;
            addlog("%s: Handschellen angelegt", name(who));
            break;
        case ZIGA:
            if (f->hp >= f->maxhp) return 0;
            f->hp++;
            addlog("%s raucht: +1 Leben", name(who));
            break;
        case SAEGE:
            if (f->saw) return 0;
            f->saw = 1;
            addlog("%s: Lauf abgesaegt (x2)", name(who));
            break;
        default:
            return 0;
    }
    f->items[slot] = NONE;
    if (pos >= nsh) load();
    return 1;
}

static void doShoot(int shooter, int self) {
    Fighter *s = shooter ? &dl : &pl;
    Fighter *o = shooter ? &pl : &dl;
    Fighter *t = self ? s : o;
    int live = shells[pos++];
    if (live) liveLeft--; else blankLeft--;
    int dmg = live ? (s->saw ? 2 : 1) : 0;
    s->saw = 0;
    peek[0] = peek[1] = 0;
    t->hp -= dmg;
    addlog("%s -> %s: %s%s", name(shooter), self ? "sich selbst" : name(1 - shooter),
           live ? "SCHARF" : "BLIND", dmg == 2 ? " (x2)" : "");

    if (pl.hp <= 0) { state = 2; dirty = 1; return; }
    if (dl.hp <= 0) {
        if (round_ >= 3) { state = 1; dirty = 1; return; }
        round_++;
        startRound();
        return;
    }

    int next = shooter;
    if (!(self && !live)) {
        int other = 1 - shooter;
        if (o->cuffed) {
            o->cuffed = 0;
            addlog("%s setzt aus (Handschellen)", name(other));
        } else {
            next = other;
        }
    }
    turn = next;
    if (pos >= nsh) load();
    timer = 50;
    dirty = 1;
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
    doShoot(1, self);
}

static void hearts(int hp, int max) {
    for (int i = 0; i < max; i++) printf(i < hp ? "[X]" : "[ ]");
}

static void draw(void) {
    consoleSelect(&top);
    consoleClear();
    printf("=== BUCKSHOT ROULETTE === Runde %d/3\n\n", round_);
    printf("DEALER: ");
    hearts(dl.hp, dl.maxhp);
    printf("%s%s\n", dl.cuffed ? " [Handschellen]" : "", dl.saw ? " [Saege]" : "");
    printf("  Items: ");
    for (int i = 0; i < MAXI; i++) if (dl.items[i]) printf("%s ", iname[dl.items[i]]);
    printf("\n\nDU:     ");
    hearts(pl.hp, pl.maxhp);
    printf("%s%s\n\n", pl.cuffed ? " [Handschellen]" : "", pl.saw ? " [Saege]" : "");
    printf("Patronen: %d scharf, %d blind\n", liveLeft, blankLeft);
    printf("--------------------------------------------------\n");
    for (int i = 0; i < LOGN; i++) printf("%s\n", logb[i]);

    consoleSelect(&bot);
    consoleClear();
    if (state == 1) {
        printf("\n\n  DU HAST GEWONNEN!\n\n  A = Neues Spiel\n  START = Beenden\n");
        return;
    }
    if (state == 2) {
        printf("\n\n  GAME OVER\n\n  A = Neues Spiel\n  START = Beenden\n");
        return;
    }
    printf(turn == 0 ? "DEIN ZUG\n\n" : "DEALER ZIEHT...\n\n");
    printf("Deine Items:\n");
    for (int i = 0; i < MAXI; i++) {
        printf("%s %d: %s\n", i == sel ? ">" : " ", i + 1, iname[pl.items[i]]);
    }
    printf("\nA = auf Dealer schiessen\nB = auf dich selbst schiessen\n");
    printf("Links/Rechts = Item waehlen\nX = Item benutzen\nSTART = Beenden\n");
}

int main(void) {
    gfxInitDefault();
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bot);
    srand((unsigned)time(NULL));
    newGame();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kd = hidKeysDown();
        if (kd & KEY_START) break;

        if (state != 0) {
            if (kd & KEY_A) { newGame(); dirty = 1; }
        } else if (turn == 0) {
            if (kd & KEY_RIGHT) { sel = (sel + 1) % MAXI; dirty = 1; }
            if (kd & KEY_LEFT) { sel = (sel + MAXI - 1) % MAXI; dirty = 1; }
            if (kd & KEY_X) {
                if (!pl.items[sel] || !useItem(0, pl.items[sel])) addlog("Das geht gerade nicht");
                dirty = 1;
            }
            if (kd & KEY_A) doShoot(0, 0);
            else if (kd & KEY_B) doShoot(0, 1);
        } else {
            if (timer > 0) timer--;
            else {
                dealerAct();
                timer = 50;
                dirty = 1;
            }
        }

        if (dirty) { draw(); dirty = 0; }
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
    gfxExit();
    return 0;
}
