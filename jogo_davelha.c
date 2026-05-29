#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum {
    VAZIO = 0,
    JOGADOR_X,
    JOGADOR_O
} CelulaEstado;

typedef enum {
    TELA_MENU = 0,
    TELA_JOGO,
    TELA_PLACAR,
    TELA_RANKING
} TelaEstado;

typedef enum {
    MODO_PVP = 0,
    MODO_CPU
} ModoJogo;

#define MAX_RANKING  50
#define ARQ_PLACAR  "velha_placar.bin"

typedef struct {
    int          linha, coluna;
    CelulaEstado estado;
} Celula;

typedef struct {
    char nome[24];        /* vetor de char */
    int  vitorias, derrotas, empates;
} Jogador;

typedef struct {
    Celula       tabuleiro[3][3]; /* matriz de estruturas */
    Jogador* jogadores;       /* vetor de estruturas (dinamico) */
    int          num_jogadores;
    CelulaEstado turno;
    int          jogadas, encerrado, vencedor;
    ModoJogo     modo;
    TelaEstado   tela;
    char         input[2][24];
    int          input_len[2], foco_input;
    ALLEGRO_BITMAP* img_fundo;
    ALLEGRO_BITMAP* img_tabuleiro;
    ALLEGRO_BITMAP* img_x;
    ALLEGRO_BITMAP* img_o;
    Jogador      ranking[MAX_RANKING]; /* vetor de estruturas fixo */
    int          total_ranking;
} Jogo;

static ALLEGRO_DISPLAY* g_disp = NULL;
static ALLEGRO_EVENT_QUEUE* g_fila = NULL;
static ALLEGRO_TIMER* g_timer = NULL;
static ALLEGRO_FONT* g_fnt = NULL;
static ALLEGRO_FONT* g_fnt_m = NULL;
static ALLEGRO_FONT* g_fnt_g = NULL;

static ALLEGRO_COLOR C_TEXTO, C_SOMBRA, C_BTN, C_BTN_H,
C_CERTO, C_ERRADO, C_DICA;

int str_len(const char* s) {
    int n = 0;
    while (s[n])
        n++;
    return n;
}

void str_cpy(char* destino, const char* origem, int tamanho_max) {
    int i = 0;
    while (origem[i] && i < tamanho_max - 1) {
        destino[i] = origem[i];
        i++;
    }
    destino[i] = '\0';
}

void str_cat(char* destino, const char* origem, int tamanho_max) {
    int n = str_len(destino);
    int i = 0;
    while (origem[i] && n + i < tamanho_max - 1) {
        destino[n + i] = origem[i];
        i++;
    }
    destino[n + i] = '\0';
}

int str_cmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *(unsigned char*)a - *(unsigned char*)b;
}

Jogador* aloc_jogadores(int quantidade) {
    return (Jogador*)malloc(quantidade * sizeof(Jogador));
}

int buscar_jogador(Jogador* vetor_jogadores, int total_jogadores, const char* nome_buscado) {
    for (int posicao = 0; posicao < total_jogadores; posicao++) {
        if (str_cmp(vetor_jogadores[posicao].nome, nome_buscado) == 0)
            return posicao;
    }
    return -1; /* nao encontrado */
}

void salvar_ranking(Jogo* jogo_atual) {
    FILE* arquivo_placar = fopen(ARQ_PLACAR, "wb");
    if (arquivo_placar == NULL) {
        printf("Erro ao abrir o arquivo!\n");
        return;
    }
    fwrite(&jogo_atual->total_ranking, sizeof(int), 1, arquivo_placar);
    fwrite(jogo_atual->ranking, sizeof(Jogador), jogo_atual->total_ranking, arquivo_placar);
    fclose(arquivo_placar);
}

void carregar_ranking(Jogo* jogo_atual) {
    FILE* arquivo_placar = fopen(ARQ_PLACAR, "rb");
    if (arquivo_placar == NULL) {
        jogo_atual->total_ranking = 0;
        return;
    }
    fread(&jogo_atual->total_ranking, sizeof(int), 1, arquivo_placar);
    if (jogo_atual->total_ranking > MAX_RANKING)
        jogo_atual->total_ranking = MAX_RANKING;
    fread(jogo_atual->ranking, sizeof(Jogador), jogo_atual->total_ranking, arquivo_placar);
    fclose(arquivo_placar);
}

/* atualiza ou cadastra jogador no ranking usando busca linear */
void atualizar_ranking(Jogo* jogo_atual, const char* nome, int vitorias, int derrotas, int empates) {
    /* busca linear: verifica se jogador ja existe */
    int posicao = buscar_jogador(jogo_atual->ranking, jogo_atual->total_ranking, nome);

    if (posicao >= 0) {
        /* jogador encontrado — acumula estatisticas */
        jogo_atual->ranking[posicao].vitorias += vitorias;
        jogo_atual->ranking[posicao].derrotas += derrotas;
        jogo_atual->ranking[posicao].empates += empates;
    }
    else {
        /* jogador nao encontrado — cadastra novo */
        if (jogo_atual->total_ranking < MAX_RANKING) {
            Jogador* novo = &jogo_atual->ranking[jogo_atual->total_ranking++];
            str_cpy(novo->nome, nome, sizeof(novo->nome));
            novo->vitorias = vitorias;
            novo->derrotas = derrotas;
            novo->empates = empates;
        }
    }
    salvar_ranking(jogo_atual);
}

void ordenar_jogadores(Jogador* vetor_jogadores, int total_jogadores) {
    for (int passagem = 0; passagem < total_jogadores - 1; passagem++) {
        for (int posicao_atual = 0; posicao_atual < total_jogadores - passagem - 1; posicao_atual++) {
            if (vetor_jogadores[posicao_atual].vitorias < vetor_jogadores[posicao_atual + 1].vitorias) {
                /* troca os dois jogadores de posicao */
                Jogador jogador_temp = vetor_jogadores[posicao_atual];
                vetor_jogadores[posicao_atual] = vetor_jogadores[posicao_atual + 1];
                vetor_jogadores[posicao_atual + 1] = jogador_temp;
            }
        }
    }
}

void init_tabuleiro(Jogo* j) {
    for (int i = 0; i < 3; i++) {
        for (int k = 0; k < 3; k++) {
            Celula* c = &j->tabuleiro[i][k]; /* ponteiro de estrutura */
            c->linha = i;
            c->coluna = k;
            c->estado = VAZIO;
        }
    }
    j->turno = JOGADOR_X;
    j->jogadas = 0;
    j->encerrado = 0;
    j->vencedor = 0;
}

int verificar_vencedor(Jogo* j) {
    CelulaEstado b[3][3]; /* matriz local */
    for (int i = 0; i < 3; i++)
        for (int k = 0; k < 3; k++)
            b[i][k] = j->tabuleiro[i][k].estado;

    /* verifica linhas e colunas */
    for (int i = 0; i < 3; i++) {
        if (b[i][0] && b[i][0] == b[i][1] && b[i][1] == b[i][2])
            return b[i][0];
        if (b[0][i] && b[0][i] == b[1][i] && b[1][i] == b[2][i])
            return b[0][i];
    }
    /* verifica diagonais */
    if (b[0][0] && b[0][0] == b[1][1] && b[1][1] == b[2][2])
        return b[0][0];
    if (b[0][2] && b[0][2] == b[1][1] && b[1][1] == b[2][0])
        return b[0][2];

    if (j->jogadas == 9) return 3; /* empate */
    return 0;
}

   /* Algoritmo: tenta vencer, bloquear, centro ou livre */

void cpu_jogar(Jogo* j) {
    int melhor_linha = -1, melhor_coluna = -1;

    /* 1. tenta vencer */
    for (int i = 0; i < 3 && melhor_linha < 0; i++) {
        for (int k = 0; k < 3 && melhor_linha < 0; k++) {
            if (j->tabuleiro[i][k].estado == VAZIO) {
                j->tabuleiro[i][k].estado = JOGADOR_O;
                if (verificar_vencedor(j) == JOGADOR_O) {
                    melhor_linha = i;
                    melhor_coluna = k;
                }
                j->tabuleiro[i][k].estado = VAZIO;
            }
        }
    }
    /* 2. bloqueia X */
    for (int i = 0; i < 3 && melhor_linha < 0; i++) {
        for (int k = 0; k < 3 && melhor_linha < 0; k++) {
            if (j->tabuleiro[i][k].estado == VAZIO) {
                j->tabuleiro[i][k].estado = JOGADOR_X;
                if (verificar_vencedor(j) == JOGADOR_X) {
                    melhor_linha = i;
                    melhor_coluna = k;
                }
                j->tabuleiro[i][k].estado = VAZIO;
            }
        }
    }
    /* 3. ocupa o centro */
    if (melhor_linha < 0 && j->tabuleiro[1][1].estado == VAZIO) {
        melhor_linha = 1;
        melhor_coluna = 1;
    }
    /* 4. qualquer celula livre */
    for (int i = 0; i < 3 && melhor_linha < 0; i++) {
        for (int k = 0; k < 3 && melhor_linha < 0; k++) {
            if (j->tabuleiro[i][k].estado == VAZIO) {
                melhor_linha = i;
                melhor_coluna = k;
            }
        }
    }
    if (melhor_linha >= 0)
        j->tabuleiro[melhor_linha][melhor_coluna].estado = JOGADOR_O;
}


void registrar_resultado(Jogo* jogo_atual) {
    Jogador* jogador_x = &jogo_atual->jogadores[0]; /* ponteiro de estrutura */
    Jogador* jogador_o = &jogo_atual->jogadores[1];

    if (jogo_atual->vencedor == 1) {
        jogador_x->vitorias++;
        jogador_o->derrotas++;
    }
    else if (jogo_atual->vencedor == 2) {
        jogador_o->vitorias++;
        jogador_x->derrotas++;
    }
    else {
        jogador_x->empates++;
        jogador_o->empates++;
    }

    /* atualiza ranking global com busca linear */
    atualizar_ranking(jogo_atual, jogador_x->nome,
        jogo_atual->vencedor == 1 ? 1 : 0,
        jogo_atual->vencedor == 2 ? 1 : 0,
        jogo_atual->vencedor == 3 ? 1 : 0);
    atualizar_ranking(jogo_atual, jogador_o->nome,
        jogo_atual->vencedor == 2 ? 1 : 0,
        jogo_atual->vencedor == 1 ? 1 : 0,
        jogo_atual->vencedor == 3 ? 1 : 0);
}

void fazer_jogada(Jogo* j, int linha, int coluna) {
    if (j->encerrado) return;

    Celula* celula = &j->tabuleiro[linha][coluna]; /* ponteiro de estrutura */
    if (celula->estado != VAZIO) return;

    celula->estado = j->turno;
    j->jogadas++;

    int resultado = verificar_vencedor(j);
    if (resultado) {
        j->encerrado = 1;
        j->vencedor = resultado;
        registrar_resultado(j);
        return;
    }

    j->turno = (j->turno == JOGADOR_X) ? JOGADOR_O : JOGADOR_X;

    if (j->modo == MODO_CPU && j->turno == JOGADOR_O) {
        cpu_jogar(j);
        j->jogadas++;
        resultado = verificar_vencedor(j);
        if (resultado) {
            j->encerrado = 1;
            j->vencedor = resultado;
            registrar_resultado(j);
            return;
        }
        j->turno = JOGADOR_X;
    }
}

void carregar_assets(Jogo* j) {
    j->img_fundo = al_load_bitmap("assets_velha/fundo.jpg");
    j->img_tabuleiro = al_load_bitmap("assets_velha/tabuleiro.png");
    j->img_x = al_load_bitmap("assets_velha/peca_x.png");
    j->img_o = al_load_bitmap("assets_velha/peca_o.png");
}

void liberar_assets(Jogo* j) {
    if (j->img_fundo)     al_destroy_bitmap(j->img_fundo);
    if (j->img_tabuleiro) al_destroy_bitmap(j->img_tabuleiro);
    if (j->img_x)         al_destroy_bitmap(j->img_x);
    if (j->img_o)         al_destroy_bitmap(j->img_o);
}

void draw_bmp_fit(ALLEGRO_BITMAP* bitmap, float x, float y, float largura, float altura) {
    if (!bitmap) return;
    al_draw_scaled_bitmap(bitmap, 0, 0,
        al_get_bitmap_width(bitmap), al_get_bitmap_height(bitmap),
        x, y, largura, altura, 0);
}

void draw_bmp_c(ALLEGRO_BITMAP* bitmap, float centro_x, float centro_y, float tamanho) {
    if (!bitmap) return;
    int bw = al_get_bitmap_width(bitmap);
    int bh = al_get_bitmap_height(bitmap);
    al_draw_scaled_bitmap(bitmap, 0, 0, bw, bh,
        centro_x - tamanho / 2,
        centro_y - tamanho / 2,
        tamanho, tamanho, 0);
}

void txt_s(ALLEGRO_FONT* fonte, float x, float y, int alinhamento, ALLEGRO_COLOR cor, const char* texto) {
    al_draw_text(fonte, C_SOMBRA, x + 2, y + 2, alinhamento, texto);
    al_draw_text(fonte, cor, x, y, alinhamento, texto);
}

void draw_painel(float x, float y, float largura, float altura) {
    al_draw_filled_rounded_rectangle(x, y, x + largura, y + altura, 10, 10, C_BTN);
    al_draw_rounded_rectangle(x, y, x + largura, y + altura, 10, 10, al_map_rgb(180, 130, 60), 3);
}

typedef struct {
    float x, y, largura, altura;
    const char* texto;
} Botao;

static Botao botoes_menu[] = {
    {0, 0, 700, 90, "Humano vs Humano"},
    {0, 0, 700, 90, "Humano vs CPU"},
    {0, 0, 700, 90, "Ver Placar"},
    {0, 0, 700, 90, "Ver Ranking"},
    {0, 0, 700, 90, "Sair"}
};
static int botao_hover = -1;

int ponto_dentro_botao(Botao* b, float mouse_x, float mouse_y) {
    return mouse_x >= b->x && mouse_x <= b->x + b->largura &&
        mouse_y >= b->y && mouse_y <= b->y + b->altura;
}

void desenhar_botao(Botao* b, int com_hover) {
    ALLEGRO_COLOR cor_fundo = com_hover ? C_BTN_H : C_BTN;
    al_draw_filled_rounded_rectangle(b->x, b->y, b->x + b->largura, b->y + b->altura, 10, 10, cor_fundo);
    al_draw_rounded_rectangle(b->x, b->y, b->x + b->largura, b->y + b->altura, 10, 10, al_map_rgb(180, 130, 60), 3);
    float pos_y_texto = b->y + (b->altura - al_get_font_line_height(g_fnt_m)) / 2.0f;
    txt_s(g_fnt_m, b->x + b->largura / 2, pos_y_texto, ALLEGRO_ALIGN_CENTRE, al_map_rgb(255, 220, 100), b->texto);
}

void desenhar_menu(Jogo* j) {
    int largura_tela = al_get_display_width(g_disp);
    int altura_tela = al_get_display_height(g_disp);
    draw_bmp_fit(j->img_fundo, 0, 0, largura_tela, altura_tela);
    al_draw_filled_rectangle(0, 0, largura_tela, altura_tela, al_map_rgba(0, 0, 0, 130));

    float centro_x = largura_tela / 2.0f;
    float centro_y = altura_tela / 2.0f;

    /* titulo */
    draw_painel(centro_x - 675, altura_tela * 0.02f, 1350, 130);
    float altura_fonte = al_get_font_line_height(g_fnt_g);
    txt_s(g_fnt_g, centro_x,
        altura_tela * 0.02f + (130 - altura_fonte) / 2.0f,
        ALLEGRO_ALIGN_CENTRE, al_map_rgb(255, 220, 100),
        "JOGO DA VELHA MEDIEVAL");

    /* botoes centralizados */
    float altura_botao = 90;
    float espaco_botoes = 20;
    float total_altura = 5 * altura_botao + 4 * espaco_botoes;
    float pos_y_inicial = centro_y - total_altura / 2.0f + 80;

    for (int i = 0; i < 5; i++) {
        botoes_menu[i].x = centro_x - 350;
        botoes_menu[i].y = pos_y_inicial + i * (altura_botao + espaco_botoes);
        desenhar_botao(&botoes_menu[i], botao_hover == i);
    }
}

void desenhar_jogo(Jogo* j) {
    int largura_tela = al_get_display_width(g_disp);
    int altura_tela = al_get_display_height(g_disp);
    draw_bmp_fit(j->img_fundo, 0, 0, largura_tela, altura_tela);
    al_draw_filled_rectangle(0, 0, largura_tela, altura_tela, al_map_rgba(0, 0, 0, 80));

    /* tabuleiro */
    int tamanho_tabuleiro = (int)(altura_tela * 0.62f);
    int pos_x_tabuleiro = (int)(largura_tela * 0.18f);
    int pos_y_tabuleiro = (int)(altura_tela * 0.05f);
    draw_bmp_fit(j->img_tabuleiro, pos_x_tabuleiro, pos_y_tabuleiro, tamanho_tabuleiro, tamanho_tabuleiro);

    /* pecas X e O */
    float tamanho_celula = tamanho_tabuleiro / 3.0f;
    for (int i = 0; i < 3; i++) {
        for (int k = 0; k < 3; k++) {
            float centro_x_peca = pos_x_tabuleiro + k * tamanho_celula + tamanho_celula / 2.0f;
            float centro_y_peca = pos_y_tabuleiro + i * tamanho_celula + tamanho_celula / 2.0f;
            float tamanho_peca = tamanho_celula * 0.78f;

            if (j->tabuleiro[i][k].estado == JOGADOR_X)
                draw_bmp_c(j->img_x, centro_x_peca, centro_y_peca, tamanho_peca);
            else if (j->tabuleiro[i][k].estado == JOGADOR_O)
                draw_bmp_c(j->img_o, centro_x_peca, centro_y_peca, tamanho_peca);
        }
    }

    /* painel placar lateral */
    float largura_painel = 360;
    float altura_painel = altura_tela * 0.72f;
    float pos_x_painel = largura_tela * 0.72f;
    float pos_y_painel = (altura_tela - altura_painel) / 2.0f;
    float centro_x_painel = pos_x_painel + largura_painel / 2.0f;

    draw_painel(pos_x_painel, pos_y_painel, largura_painel, altura_painel);
    txt_s(g_fnt_m, centro_x_painel, pos_y_painel + 10,
        ALLEGRO_ALIGN_CENTRE, al_map_rgb(255, 220, 100), "PLACAR");
    al_draw_line(pos_x_painel + 20, pos_y_painel + 80,
        pos_x_painel + largura_painel - 20, pos_y_painel + 80,
        al_map_rgb(180, 130, 60), 2);

    /* labels dependem do modo de jogo */
    char label_x[40], label_o[40];
    if (j->modo == MODO_PVP) {
        str_cpy(label_x, j->jogadores[0].nome, sizeof(label_x));
        str_cat(label_x, " (X)", sizeof(label_x));
        str_cpy(label_o, j->jogadores[1].nome, sizeof(label_o));
        str_cat(label_o, " (O)", sizeof(label_o));
    }
    else {
        str_cpy(label_x, "Jogador (X)", sizeof(label_x));
        str_cpy(label_o, "CPU (O)", sizeof(label_o));
    }

    /* estatisticas jogador X */
    float pos_y_stats = pos_y_painel + 95;
    float espaco_linha = 48;
    char  texto_stat[48];

    txt_s(g_fnt, centro_x_painel, pos_y_stats,
        ALLEGRO_ALIGN_CENTRE, al_map_rgb(255, 220, 100), label_x);

    snprintf(texto_stat, sizeof(texto_stat), "Vitorias: %d", j->jogadores[0].vitorias);
    txt_s(g_fnt, centro_x_painel, pos_y_stats + espaco_linha + 14, ALLEGRO_ALIGN_CENTRE, C_CERTO, texto_stat);

    snprintf(texto_stat, sizeof(texto_stat), "Derrotas: %d", j->jogadores[0].derrotas);
    txt_s(g_fnt, centro_x_painel, pos_y_stats + espaco_linha * 2 + 14, ALLEGRO_ALIGN_CENTRE, C_ERRADO, texto_stat);

    snprintf(texto_stat, sizeof(texto_stat), "Empates:  %d", j->jogadores[0].empates);
    txt_s(g_fnt, centro_x_painel, pos_y_stats + espaco_linha * 3 + 14, ALLEGRO_ALIGN_CENTRE, C_DICA, texto_stat);

    /* linha divisoria */
    al_draw_line(pos_x_painel + 20, pos_y_painel + altura_painel / 2 + 20,
        pos_x_painel + largura_painel - 20, pos_y_painel + altura_painel / 2 + 20,
        al_map_rgb(180, 130, 60), 2);

    /* estatisticas jogador O */
    float pos_y_stats_o = pos_y_painel + altura_painel / 2 + 40;

    txt_s(g_fnt, centro_x_painel, pos_y_stats_o,
        ALLEGRO_ALIGN_CENTRE, al_map_rgb(255, 220, 100), label_o);

    snprintf(texto_stat, sizeof(texto_stat), "Vitorias: %d", j->jogadores[1].vitorias);
    txt_s(g_fnt, centro_x_painel, pos_y_stats_o + espaco_linha + 14, ALLEGRO_ALIGN_CENTRE, C_CERTO, texto_stat);

    snprintf(texto_stat, sizeof(texto_stat), "Derrotas: %d", j->jogadores[1].derrotas);
    txt_s(g_fnt, centro_x_painel, pos_y_stats_o + espaco_linha * 2 + 14, ALLEGRO_ALIGN_CENTRE, C_ERRADO, texto_stat);

    snprintf(texto_stat, sizeof(texto_stat), "Empates:  %d", j->jogadores[1].empates);
    txt_s(g_fnt, centro_x_painel, pos_y_stats_o + espaco_linha * 3 + 14, ALLEGRO_ALIGN_CENTRE, C_DICA, texto_stat);

    /* banner de status abaixo do tabuleiro */
    float largura_banner = 700;
    float altura_banner = 200;
    float pos_x_banner = largura_tela * 0.18f;
    float pos_y_banner = pos_y_tabuleiro + tamanho_tabuleiro + 15;
    float centro_x_banner = pos_x_banner + largura_banner / 2.0f;

    draw_painel(pos_x_banner, pos_y_banner, largura_banner, altura_banner);

    if (j->encerrado) {
        if (j->vencedor == 3) {
            txt_s(g_fnt_m, centro_x_banner, pos_y_banner + 20,
                ALLEGRO_ALIGN_CENTRE, al_map_rgb(255, 220, 100), "EMPATE!");
        }
        else {
            char texto_vencedor[48] = "";
            str_cpy(texto_vencedor, (j->vencedor == 1) ? label_x : label_o, sizeof(texto_vencedor));
            str_cat(texto_vencedor, " VENCEU!", sizeof(texto_vencedor));
            txt_s(g_fnt_m, centro_x_banner, pos_y_banner + 20,
                ALLEGRO_ALIGN_CENTRE, C_CERTO, texto_vencedor);
        }
        txt_s(g_fnt, centro_x_banner, pos_y_banner + 120,
            ALLEGRO_ALIGN_CENTRE, C_TEXTO, "[R] Novo  [P] Placar  [ESC] Menu");
    }
    else {
        char texto_turno[64] = "Vez de: ";
        if (j->modo == MODO_PVP) {
            char* nome_turno = j->turno == JOGADOR_X ? j->jogadores[0].nome : j->jogadores[1].nome;
            char* simb_turno = j->turno == JOGADOR_X ? " (X)" : " (O)";
            str_cat(texto_turno, nome_turno, sizeof(texto_turno));
            str_cat(texto_turno, simb_turno, sizeof(texto_turno));
        }
        else {
            str_cat(texto_turno, j->turno == JOGADOR_X ? "Jogador (X)" : "CPU (O)", sizeof(texto_turno));
        }

        txt_s(g_fnt_m, centro_x_banner, pos_y_banner + 20,
            ALLEGRO_ALIGN_CENTRE, al_map_rgb(255, 220, 100), texto_turno);
        txt_s(g_fnt, centro_x_banner, pos_y_banner + 120,
            ALLEGRO_ALIGN_CENTRE, C_DICA, "[ESC] Menu");
    }
}

void desenhar_placar(Jogo* j) {
    int largura_tela = al_get_display_width(g_disp);
    int altura_tela = al_get_display_height(g_disp);
    draw_bmp_fit(j->img_fundo, 0, 0, largura_tela, altura_tela);
    al_draw_filled_rectangle(0, 0, largura_tela, altura_tela, al_map_rgba(0, 0, 0, 150));

    float centro_x = largura_tela / 2.0f;

    /* titulo */
    draw_painel(centro_x - 300, altura_tela * 0.04f, 600, 80);
    txt_s(g_fnt_m, centro_x, altura_tela * 0.04f + 10,
        ALLEGRO_ALIGN_CENTRE, al_map_rgb(255, 220, 100), "PLACAR");

    /* painel de estatisticas dos jogadores */
    float largura_painel_stats = 700;
    float altura_painel_stats = 280;
    float pos_x_stats = centro_x - largura_painel_stats / 2;
    float pos_y_stats = altura_tela * 0.04f + 100;
    draw_painel(pos_x_stats, pos_y_stats, largura_painel_stats, altura_painel_stats);

    /* copia e ordena — nao altera o vetor original */
    Jogador* jogadores_ordenados = aloc_jogadores(j->num_jogadores);
    if (jogadores_ordenados) {
        for (int i = 0; i < j->num_jogadores; i++)
            jogadores_ordenados[i] = j->jogadores[i];

        ordenar_jogadores(jogadores_ordenados, j->num_jogadores);

        float largura_coluna = largura_painel_stats / 2.0f;
        for (int i = 0; i < 2 && i < j->num_jogadores; i++) {
            float centro_coluna = pos_x_stats + largura_coluna * i + largura_coluna / 2.0f;
            char  texto_stat[48];

            txt_s(g_fnt_m, centro_coluna, pos_y_stats + 18,
                ALLEGRO_ALIGN_CENTRE, al_map_rgb(255, 220, 100), jogadores_ordenados[i].nome);

            snprintf(texto_stat, sizeof(texto_stat), "Vitorias: %d", jogadores_ordenados[i].vitorias);
            txt_s(g_fnt, centro_coluna, pos_y_stats + 90, ALLEGRO_ALIGN_CENTRE, C_CERTO, texto_stat);

            snprintf(texto_stat, sizeof(texto_stat), "Derrotas: %d", jogadores_ordenados[i].derrotas);
            txt_s(g_fnt, centro_coluna, pos_y_stats + 140, ALLEGRO_ALIGN_CENTRE, C_ERRADO, texto_stat);

            snprintf(texto_stat, sizeof(texto_stat), "Empates:  %d", jogadores_ordenados[i].empates);
            txt_s(g_fnt, centro_coluna, pos_y_stats + 190, ALLEGRO_ALIGN_CENTRE, C_DICA, texto_stat);
        }
        al_draw_line(centro_x, pos_y_stats + 10,
            centro_x, pos_y_stats + altura_painel_stats - 10,
            al_map_rgb(180, 130, 60), 2);
    }

    /* painel vencedor */
    float pos_y_vencedor = pos_y_stats + altura_painel_stats + 20;
    float altura_vencedor = 260;
    draw_painel(pos_x_stats, pos_y_vencedor, largura_painel_stats, altura_vencedor);
    txt_s(g_fnt_m, centro_x, pos_y_vencedor + 3,
        ALLEGRO_ALIGN_CENTRE, al_map_rgb(255, 220, 100), "Vencedor");
    al_draw_line(pos_x_stats + 20, pos_y_vencedor + 75,
        pos_x_stats + largura_painel_stats - 20, pos_y_vencedor + 75,
        al_map_rgb(180, 130, 60), 2);

    if (jogadores_ordenados && jogadores_ordenados[0].vitorias > 0) {
        txt_s(g_fnt_m, centro_x, pos_y_vencedor + 82,
            ALLEGRO_ALIGN_CENTRE, C_CERTO, jogadores_ordenados[0].nome);
        char texto_vit[48];
        snprintf(texto_vit, sizeof(texto_vit), "Vitorias: %d", jogadores_ordenados[0].vitorias);
        txt_s(g_fnt_m, centro_x, pos_y_vencedor + 165,
            ALLEGRO_ALIGN_CENTRE, C_CERTO, texto_vit);
    }
    else {
        txt_s(g_fnt_m, centro_x, pos_y_vencedor + 110,
            ALLEGRO_ALIGN_CENTRE, C_DICA, "Nenhuma partida ainda.");
    }
    free(jogadores_ordenados);

    /* botao voltar */
    float largura_botao_voltar = 300;
    float altura_botao_voltar = 55;
    float pos_x_botao_voltar = centro_x - 150;
    float pos_y_botao_voltar = pos_y_vencedor + altura_vencedor + 50;
    draw_painel(pos_x_botao_voltar, pos_y_botao_voltar, largura_botao_voltar, altura_botao_voltar);
    txt_s(g_fnt, centro_x, pos_y_botao_voltar + 10,
        ALLEGRO_ALIGN_CENTRE, al_map_rgb(255, 220, 100), "[ESC] Voltar");
}

void desenhar_ranking(Jogo* j) {
    int largura_tela = al_get_display_width(g_disp);
    int altura_tela = al_get_display_height(g_disp);
    draw_bmp_fit(j->img_fundo, 0, 0, largura_tela, altura_tela);
    al_draw_filled_rectangle(0, 0, largura_tela, altura_tela, al_map_rgba(0, 0, 0, 160));

    float centro_x = largura_tela / 2.0f;

    /* titulo */
    draw_painel(centro_x - 350, altura_tela * 0.03f, 700, 80);
    txt_s(g_fnt_m, centro_x, altura_tela * 0.03f + 10,
        ALLEGRO_ALIGN_CENTRE, al_map_rgb(255, 220, 100), "RANKING GLOBAL");

    /* painel da lista */
    float pos_x_lista = centro_x - 500;
    float pos_y_lista = altura_tela * 0.03f + 100;
    float largura_lista = 1000;
    float altura_lista = altura_tela * 0.70f;
    draw_painel(pos_x_lista, pos_y_lista, largura_lista, altura_lista);

    /* cabecalho */
    float col1 = pos_x_lista + 80;
    float col2 = pos_x_lista + 300;
    float col3 = pos_x_lista + 520;
    float col4 = pos_x_lista + 720;
    float col5 = pos_x_lista + 900;

    txt_s(g_fnt, col1, pos_y_lista + 15, ALLEGRO_ALIGN_LEFT, al_map_rgb(255, 220, 100), "#");
    txt_s(g_fnt, col2, pos_y_lista + 15, ALLEGRO_ALIGN_CENTRE, al_map_rgb(255, 220, 100), "Nome");
    txt_s(g_fnt, col3, pos_y_lista + 15, ALLEGRO_ALIGN_CENTRE, C_CERTO, "Vitorias");
    txt_s(g_fnt, col4, pos_y_lista + 15, ALLEGRO_ALIGN_CENTRE, C_ERRADO, "Derrotas");
    txt_s(g_fnt, col5, pos_y_lista + 15, ALLEGRO_ALIGN_CENTRE, C_DICA, "Empates");
    al_draw_line(pos_x_lista + 20, pos_y_lista + 58,
        pos_x_lista + largura_lista - 20, pos_y_lista + 58,
        al_map_rgb(180, 130, 60), 2);

    if (j->total_ranking == 0) {
        txt_s(g_fnt_m, centro_x, pos_y_lista + altura_lista / 2,
            ALLEGRO_ALIGN_CENTRE, C_DICA, "Nenhum jogador registrado ainda.");
    }
    else {
        /* copia e ordena para exibir */
        Jogador copia_ranking[MAX_RANKING];
        for (int i = 0; i < j->total_ranking; i++)
            copia_ranking[i] = j->ranking[i];
        ordenar_jogadores(copia_ranking, j->total_ranking);

        int max_exibir = j->total_ranking < 10 ? j->total_ranking : 10;
        for (int i = 0; i < max_exibir; i++) {
            float pos_y_linha = pos_y_lista + 70 + i * 52;
            char  texto[48];

            /* destaque para o 1o lugar */
            ALLEGRO_COLOR cor_nome = i == 0 ? al_map_rgb(255, 220, 100) : C_TEXTO;

            snprintf(texto, sizeof(texto), "%d.", i + 1);
            txt_s(g_fnt, col1, pos_y_linha, ALLEGRO_ALIGN_LEFT, cor_nome, texto);
            txt_s(g_fnt, col2, pos_y_linha, ALLEGRO_ALIGN_CENTRE, cor_nome, copia_ranking[i].nome);

            snprintf(texto, sizeof(texto), "%d", copia_ranking[i].vitorias);
            txt_s(g_fnt, col3, pos_y_linha, ALLEGRO_ALIGN_CENTRE, C_CERTO, texto);

            snprintf(texto, sizeof(texto), "%d", copia_ranking[i].derrotas);
            txt_s(g_fnt, col4, pos_y_linha, ALLEGRO_ALIGN_CENTRE, C_ERRADO, texto);

            snprintf(texto, sizeof(texto), "%d", copia_ranking[i].empates);
            txt_s(g_fnt, col5, pos_y_linha, ALLEGRO_ALIGN_CENTRE, C_DICA, texto);
        }
    }

    /* botao voltar */
    float pos_x_voltar = centro_x - 150;
    float pos_y_voltar = pos_y_lista + altura_lista + 20;
    draw_painel(pos_x_voltar, pos_y_voltar, 300, 55);
    txt_s(g_fnt, centro_x, pos_y_voltar + 10,
        ALLEGRO_ALIGN_CENTRE, al_map_rgb(255, 220, 100), "[ESC] Voltar");
}

Jogo* criar_jogo(void) {
    Jogo* j = (Jogo*)malloc(sizeof(Jogo)); /* alocacao dinamica de estrutura */
    if (!j) return NULL;

    j->num_jogadores = 2;
    j->jogadores = aloc_jogadores(2);
    j->tela = TELA_MENU;
    j->modo = MODO_PVP;
    j->foco_input = 0;

    str_cpy(j->jogadores[0].nome, "", sizeof(j->jogadores[0].nome));
    str_cpy(j->jogadores[1].nome, "", sizeof(j->jogadores[1].nome));
    j->jogadores[0].vitorias = j->jogadores[0].derrotas = j->jogadores[0].empates = 0;
    j->jogadores[1].vitorias = j->jogadores[1].derrotas = j->jogadores[1].empates = 0;

    str_cpy(j->input[0], "", sizeof(j->input[0]));
    j->input_len[0] = 0;
    str_cpy(j->input[1], "", sizeof(j->input[1]));
    j->input_len[1] = 0;

    j->img_fundo = j->img_tabuleiro = j->img_x = j->img_o = NULL;
    j->total_ranking = 0;
    init_tabuleiro(j);
    return j;
}

void destruir_jogo(Jogo* j) {
    if (!j) return;
    liberar_assets(j);
    free(j->jogadores);
    free(j);
}

void iniciar_partida(Jogo* j, ModoJogo modo) {
    j->modo = modo;

    if (j->input_len[0] > 0)
        str_cpy(j->jogadores[0].nome, j->input[0], sizeof(j->jogadores[0].nome));

    if (modo == MODO_CPU)
        str_cpy(j->jogadores[1].nome, "CPU", sizeof(j->jogadores[1].nome));
    else if (j->input_len[1] > 0)
        str_cpy(j->jogadores[1].nome, j->input[1], sizeof(j->jogadores[1].nome));

    j->jogadores[0].vitorias = j->jogadores[0].derrotas = j->jogadores[0].empates = 0;
    j->jogadores[1].vitorias = j->jogadores[1].derrotas = j->jogadores[1].empates = 0;

    init_tabuleiro(j);
    j->tela = TELA_JOGO;
}

int main(void) {
    srand((unsigned)time(NULL));

    /* pede nomes pelo terminal antes de abrir a janela */
    char nome1[24], nome2[24];
    printf("Digite o nome do Jogador 1: ");
    if (fgets(nome1, sizeof(nome1), stdin)) {
        int len = str_len(nome1);
        if (len > 0 && nome1[len - 1] == '\n') nome1[len - 1] = '\0';
    }
    if (str_len(nome1) == 0) str_cpy(nome1, "Jogador 1", sizeof(nome1));

    printf("Digite o nome do Jogador 2 (ou ENTER para CPU): ");
    if (fgets(nome2, sizeof(nome2), stdin)) {
        int len = str_len(nome2);
        if (len > 0 && nome2[len - 1] == '\n') nome2[len - 1] = '\0';
    }
    int modo_cpu = (str_len(nome2) == 0);
    if (modo_cpu) str_cpy(nome2, "CPU", sizeof(nome2));

    al_init();
    al_install_keyboard();
    al_install_mouse();
    al_init_font_addon();
    al_init_primitives_addon();
    al_init_image_addon();

    al_set_new_display_flags(ALLEGRO_FULLSCREEN_WINDOW);
    g_disp = al_create_display(0, 0);
    al_set_window_title(g_disp, "Jogo da Velha Medieval");

    g_timer = al_create_timer(1.0 / 60.0);
    g_fila = al_create_event_queue();
    al_register_event_source(g_fila, al_get_display_event_source(g_disp));
    al_register_event_source(g_fila, al_get_keyboard_event_source());
    al_register_event_source(g_fila, al_get_mouse_event_source());
    al_register_event_source(g_fila, al_get_timer_event_source(g_timer));

    al_init_ttf_addon();
    g_fnt_g = al_load_ttf_font("assets_velha/MedievalSharp-Bold.ttf", 96, 0);
    g_fnt_m = al_load_ttf_font("assets_velha/MedievalSharp-Bold.ttf", 64, 0);
    g_fnt = al_load_ttf_font("assets_velha/MedievalSharp-Bold.ttf", 40, 0);
    if (!g_fnt_g) g_fnt_g = al_create_builtin_font();
    if (!g_fnt_m) g_fnt_m = al_create_builtin_font();
    if (!g_fnt)   g_fnt = al_create_builtin_font();

    C_TEXTO = al_map_rgb(220, 210, 190);
    C_SOMBRA = al_map_rgba(0, 0, 0, 180);
    C_BTN = al_map_rgba(80, 50, 20, 200);
    C_BTN_H = al_map_rgba(120, 80, 30, 220);
    C_CERTO = al_map_rgb(80, 200, 100);
    C_ERRADO = al_map_rgb(200, 60, 60);
    C_DICA = al_map_rgb(160, 200, 220);

    al_start_timer(g_timer);
    Jogo* jogo = criar_jogo();
    carregar_assets(jogo);

    /* aplica nomes digitados no terminal */
    str_cpy(jogo->jogadores[0].nome, nome1, sizeof(jogo->jogadores[0].nome));
    str_cpy(jogo->jogadores[1].nome, nome2, sizeof(jogo->jogadores[1].nome));
    str_cpy(jogo->input[0], nome1, sizeof(jogo->input[0]));
    str_cpy(jogo->input[1], nome2, sizeof(jogo->input[1]));
    jogo->input_len[0] = str_len(nome1);
    jogo->input_len[1] = str_len(nome2);
    if (modo_cpu) jogo->modo = MODO_CPU;

    /* carrega ranking salvo do arquivo binario */
    carregar_ranking(jogo);

    int redraw = 1;
    int rodando = 1;

    while (rodando) {
        ALLEGRO_EVENT evento;
        al_wait_for_event(g_fila, &evento);

        if (evento.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            rodando = 0;
            break;
        }
        if (evento.type == ALLEGRO_EVENT_TIMER) {
            redraw = 1;
        }

        /* hover nos botoes do menu */
        if (evento.type == ALLEGRO_EVENT_MOUSE_AXES && jogo->tela == TELA_MENU) {
            botao_hover = -1;
            for (int i = 0; i < 5; i++)
                if (ponto_dentro_botao(&botoes_menu[i], evento.mouse.x, evento.mouse.y))
                    botao_hover = i;
        }

        /* clique do mouse */
        if (evento.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN && evento.mouse.button == 1) {
            float mouse_x = evento.mouse.x;
            float mouse_y = evento.mouse.y;

            if (jogo->tela == TELA_MENU) {
                if (ponto_dentro_botao(&botoes_menu[0], mouse_x, mouse_y))
                    iniciar_partida(jogo, MODO_PVP);
                else if (ponto_dentro_botao(&botoes_menu[1], mouse_x, mouse_y))
                    iniciar_partida(jogo, MODO_CPU);
                else if (ponto_dentro_botao(&botoes_menu[2], mouse_x, mouse_y))
                    jogo->tela = TELA_PLACAR;
                else if (ponto_dentro_botao(&botoes_menu[3], mouse_x, mouse_y))
                    jogo->tela = TELA_RANKING;
                else if (ponto_dentro_botao(&botoes_menu[4], mouse_x, mouse_y))
                    rodando = 0;

            }
            else if (jogo->tela == TELA_JOGO && !jogo->encerrado) {
                int largura_tela2 = al_get_display_width(g_disp);
                int altura_tela2 = al_get_display_height(g_disp);
                int tamanho_tabuleiro2 = (int)(altura_tela2 * 0.62f);
                int pos_x_tabuleiro2 = (int)(largura_tela2 * 0.18f);
                int pos_y_tabuleiro2 = (int)(altura_tela2 * 0.05f);
                float tamanho_celula2 = (float)tamanho_tabuleiro2 / 3.0f;

                int coluna = (int)((mouse_x - pos_x_tabuleiro2) / tamanho_celula2);
                int linha = (int)((mouse_y - pos_y_tabuleiro2) / tamanho_celula2);

                if (linha >= 0 && linha < 3 &&
                    coluna >= 0 && coluna < 3 &&
                    mouse_x >= pos_x_tabuleiro2 &&
                    mouse_y >= pos_y_tabuleiro2 &&
                    mouse_x <= pos_x_tabuleiro2 + tamanho_tabuleiro2 &&
                    mouse_y <= pos_y_tabuleiro2 + tamanho_tabuleiro2)
                    fazer_jogada(jogo, linha, coluna);
            }
        }

        /* teclado */
        if (evento.type == ALLEGRO_EVENT_KEY_CHAR) {
            int caractere = evento.keyboard.unichar;
            int tecla = evento.keyboard.keycode;

            if (jogo->tela == TELA_MENU) {
                int campo = jogo->foco_input;
                if (tecla == ALLEGRO_KEY_BACKSPACE && jogo->input_len[campo] > 0)
                    jogo->input[campo][--jogo->input_len[campo]] = '\0';
                else if (tecla == ALLEGRO_KEY_ESCAPE)
                    rodando = 0;

            }
            else if (jogo->tela == TELA_JOGO) {
                if (tecla == ALLEGRO_KEY_R)
                    init_tabuleiro(jogo);
                else if (tecla == ALLEGRO_KEY_P)
                    jogo->tela = TELA_PLACAR;
                else if (tecla == ALLEGRO_KEY_ESCAPE)
                    jogo->tela = TELA_MENU;

            }
            else if (jogo->tela == TELA_PLACAR) {
                if (tecla == ALLEGRO_KEY_ESCAPE)
                    jogo->tela = TELA_MENU;
            }
            else if (jogo->tela == TELA_RANKING) {
                if (tecla == ALLEGRO_KEY_ESCAPE)
                    jogo->tela = TELA_MENU;
            }
            (void)caractere;
        }

        /* renderizacao */
        if (redraw && al_is_event_queue_empty(g_fila)) {
            redraw = 0;
            switch (jogo->tela) {
            case TELA_MENU:    desenhar_menu(jogo);    break;
            case TELA_JOGO:    desenhar_jogo(jogo);    break;
            case TELA_PLACAR:  desenhar_placar(jogo);  break;
            case TELA_RANKING: desenhar_ranking(jogo); break;
            }
            al_flip_display();
        }
    }

    destruir_jogo(jogo);
    al_destroy_font(g_fnt);
    al_destroy_font(g_fnt_m);
    al_destroy_font(g_fnt_g);
    al_destroy_event_queue(g_fila);
    al_destroy_timer(g_timer);
    al_destroy_display(g_disp);
    return 0;
}
