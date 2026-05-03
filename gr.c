#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define mxstat 100
#define mxname 16

typedef struct {
    char name[mxname];
    bool flag;
} stat;

typedef struct {
    int time;
    bool used;
} edge;

typedef struct {
    stat stats[mxstat];
    edge edges[mxstat][mxstat];
    int count;
} gr;

typedef struct {
    int rest;
    int resl;
    int resp[mxstat];
    int curp[mxstat];
    bool check;
} info;

void newgr(gr* g) {
    if (!g) return;
    g->count = 0;
    for (int i = 0; i < mxstat; i++) {
        g->stats[i].flag = false;
        for (int j = 0; j < mxstat; j++) {
            g->edges[i][j].time = 0;
            g->edges[i][j].used = false;
        }
    }
}

int fi(const gr* g, const char* name) {
    if (!g || !name) return -1;
    for (int i = 0; i < g->count; i++) {
        if (strcmp(g->stats[i].name, name) == 0) return i;
    }
    return -1;
}

int add(gr* g, const char* name) {
    if (!g || g->count >= mxstat) return -1;
    int idx = fi(g, name);
    if (idx != -1) return idx;
    
    strncpy(g->stats[g->count].name, name, mxname-1);
    g->stats[g->count].name[mxname-1] = '\0';
    g->stats[g->count].flag = false;
    return g->count++;
}

void read(gr* g, const char* f, char* fin, int* t) {
    if (!g || !f || !fin || !t) {
        printf("ошибка: нулевой указатель\n");
        return;
    }

    FILE* file = fopen(f, "r");
    if (!file) {
        printf("ошибка: не удалось открыть файл %s\n", f);
        return;
    }

    if (fscanf(file, "%d", t) != 1) {
        printf("ошибка: не удалось прочитать время\n");
        fclose(file);
        return;
    }

    while (fgetc(file) != '\n' && !feof(file)) {}

    char from[mxname], to[mxname];
    int time;
    int num = 1;

    while (fscanf(file, "%15s %15s %d", from, to, &time) == 3) {
        num++;
        if (strcmp(to, "Airport") == 0) {
            strncpy(fin, "Airport", mxname-1);
            fin[mxname-1] = '\0';
        }
        int fromi = add(g, from);
        int toi = add(g, to);
        if (fromi != -1 && toi != -1) {
            g->edges[fromi][toi].time = time;
        }
    }
    fclose(file);

    printf("\nзагружено %d станций:\n", g->count);
    for (int i = 0; i < g->count; i++) {
        printf("%d: %s\n", i, g->stats[i].name);
    }
}

void dfs(gr* g, info* s, const char* fin, int current, int dep, int nowt, int nowc) {
    if (!g || !s || !fin) return;

    s->curp[dep] = current;

    if (strcmp(g->stats[current].name, fin) == 0 && nowc == g->count) {
        if (nowt <= s->rest || !s->check) {
            s->rest = nowt;
            s->resl = dep;
            s->check = true;
            for (int i = 0; i <= dep; i++) {
                s->resp[i] = s->curp[i];
            }
        }
        return;
    }

    for (int next = 0; next < g->count; next++) {
        if (g->edges[current][next].time > 0 && !g->edges[current][next].used) {
            bool chfin = (strcmp(g->stats[next].name, fin) == 0);
            bool fl = g->stats[next].flag;
            int station_time;
            
            if (fl || chfin) {
                station_time = 0;
            } else {
                station_time = 5;
            }
            
            int newt = nowt + g->edges[current][next].time + station_time;

            if (newt > s->rest && s->check) {
                continue;
            }

            g->edges[current][next].used = true;
            if (!fl && !chfin) {
                g->stats[next].flag = true;
            }

            int newc;
            if (fl) {
                newc = nowc;
            } else {
                newc = nowc + 1;
            }

            dfs(g, s, fin, next, dep+1, newt, newc);

            g->edges[current][next].used = false;
            if (!fl && !chfin) {
                g->stats[next].flag = false;
            }
        }
    }
}

void fr(gr* g, info* s, const char* fin, const char* beg, int t) {
    if (!g || !s || !fin || !beg) return;

    int begi = fi(g, beg);
    if (begi == -1) {
        printf("ошибка: станция '%s' не найдена\n", beg);
        return;
    }

    s->rest = t;
    s->check = false;

    g->stats[begi].flag = true;
    dfs(g, s, fin, begi, 0, 5, 1);
    g->stats[begi].flag = false;

    if (s->check) {
        printf("\nоптимальный маршрут:\n");
        for (int i = 0; i <= s->resl; i++) {
            printf("%s", g->stats[s->resp[i]].name);
            if (i < s->resl) {
                printf(" → ");
            }
        }
        printf("\nобщее время: %d минут\n", s->rest);
        printf("количество пересадок: %d\n", s->resl);
    } else {
        printf("\nне удалось найти подходящий маршрут за %d минут\n", t);
    }
}

void to_lower_case(char* str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("использование: %s <файл>\n", argv[0]);
        printf("пример: %s metro.txt\n", argv[0]);
        return 1;
    }

    gr metro;
    info state;
    char fin[mxname] = "Airport";
    int t = 0;
    char beg[mxname];
    char choice[10];

    do {
        newgr(&metro);
        read(&metro, argv[1], fin, &t);

        printf("\nвведите начальную станцию: ");
        if (scanf("%15s", beg) != 1) {
            printf("ошибка ввода\n");
            break;
        }

        fr(&metro, &state, fin, beg, t);

        printf("\nхотите попробовать другую станцию? (да/нет): ");
        scanf("%9s", choice);
        to_lower_case(choice);

    } while (strcmp(choice, "нет") != 0 && strcmp(choice, "n") != 0);

    return 0;
}
