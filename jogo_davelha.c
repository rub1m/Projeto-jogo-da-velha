#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum { VAZIO=0, JOGADOR_X, JOGADOR_O } CelulaEstado;
typedef enum { TELA_MENU=0, TELA_JOGO, TELA_PLACAR } TelaEstado;
typedef enum { MODO_PVP=0, MODO_CPU } ModoJogo;

typedef struct { float x, y; } Vec2;

typedef struct {
    Vec2         pos;     
    int          linha, coluna;
    CelulaEstado estado;
} Celula;

typedef struct {
    char nome[24];
    int  vitorias, derrotas, empates;
} Jogador;

typedef struct {
    char nome_x[24], nome_o[24];
    int  vencedor;          
    int  jogadas;
    long timestamp;
} Partida;

typedef struct {
    Celula   tabuleiro[3][3]; 
    Jogador *jogadores;       
    int      num_jogadores;
    Partida *historico;       
    int      num_partidas, cap_historico;
    CelulaEstado turno;
    int      jogadas, encerrado, vencedor;
    ModoJogo modo;
    TelaEstado tela;
    char     input[2][24];    
    int      input_len[2], foco_input;
    
    ALLEGRO_BITMAP *img_fundo;
    ALLEGRO_BITMAP *img_tabuleiro;
    ALLEGRO_BITMAP *img_x;
    ALLEGRO_BITMAP *img_o;
    ALLEGRO_BITMAP *img_pergaminho_v;
    ALLEGRO_BITMAP *img_pergaminho_h;
} Jogo;

#define W           800
#define H           600
#define TAB_SZ      480   
#define TAB_X       160    
#define TAB_Y        60    
#define CEL_SZ      148    
#define CEL_GAP      14    
#define CAP_INI       8
#define ARQ_PLACAR  "velha_placar.bin"
#define ARQ_LOG     "velha_log.txt"

static ALLEGRO_DISPLAY     *g_disp  = NULL;
static ALLEGRO_EVENT_QUEUE *g_fila  = NULL;
static ALLEGRO_TIMER       *g_timer = NULL;
static ALLEGRO_FONT        *g_fnt   = NULL;

static ALLEGRO_COLOR C_TEXTO, C_TITULO, C_SOMBRA,
                     C_BTN, C_BTN_H, C_CERTO, C_ERRADO, C_DICA;


int  str_len(const char *s){ int n=0; while(s[n])n++; return n; }
void str_cpy(char *d,const char *s,int m){ int i=0; while(s[i]&&i<m-1){d[i]=s[i];i++;} d[i]='\0'; }
void str_cat(char *d,const char *s,int m){ int n=str_len(d),i=0; while(s[i]&&n+i<m-1){d[n+i]=s[i];i++;} d[n+i]='\0'; }
int  str_cmp(const char *a,const char *b){ while(*a&&*b&&*a==*b){a++;b++;} return *(unsigned char*)a-*(unsigned char*)b; }


Jogador *aloc_jogadores(int n){ return (Jogador*)malloc(n*sizeof(Jogador)); }
Partida *aloc_historico(int n){ return (Partida*)malloc(n*sizeof(Partida)); }

void salvar_placar(Jogo *j){
    FILE *f=fopen(ARQ_PLACAR,"wb"); if(!f)return;
    fwrite(&j->num_jogadores,sizeof(int),1,f);
    fwrite(j->jogadores,sizeof(Jogador),j->num_jogadores,f);
    fwrite(&j->num_partidas,sizeof(int),1,f);
    if(j->num_partidas>0)
        fwrite(j->historico,sizeof(Partida),j->num_partidas,f);
    fclose(f);
}

void carregar_placar(Jogo *j){
    FILE *f=fopen(ARQ_PLACAR,"rb"); if(!f)return;
    int n=0; fread(&n,sizeof(int),1,f);
    if(n==j->num_jogadores){
        Jogador *tmp=aloc_jogadores(n);
        if(tmp){ fread(tmp,sizeof(Jogador),n,f);
           
            int ok=1;
            for(int i=0;i<n;i++)
                if(str_cmp(tmp[i].nome,j->jogadores[i].nome)!=0){ok=0;break;}
            if(ok){free(j->jogadores);j->jogadores=tmp;}
            else free(tmp);
        }
    }
    int np=0; fread(&np,sizeof(int),1,f);
    if(np>0&&np<=10000){
        Partida *hp=(Partida*)realloc(j->historico,np*sizeof(Partida));
        if(hp){j->historico=hp;j->num_partidas=np;j->cap_historico=np;
            fread(j->historico,sizeof(Partida),np,f);}
    }
    fclose(f);
}

void log_partida(Partida *p){
    FILE *f=fopen(ARQ_LOG,"a"); if(!f)return;
    char buf[128]="[PARTIDA] X="; str_cat(buf,p->nome_x,sizeof(buf));
    str_cat(buf," O=",sizeof(buf)); str_cat(buf,p->nome_o,sizeof(buf));
    const char *res= p->vencedor==0?"EMPATE": p->vencedor==1?p->nome_x:p->nome_o;
    fprintf(f,"%s Resultado=%s Jogadas=%d\n",buf,res,p->jogadas);
    fclose(f);
}

int buscar_jogador(Jogador *v,int n,const char *nome){
    for(int i=0;i<n;i++) if(str_cmp(v[i].nome,nome)==0)return i;
    return -1;
}

void ordenar_jogadores(Jogador *v,int n){
    for(int i=0;i<n-1;i++)
        for(int k=0;k<n-i-1;k++)
            if(v[k].vitorias<v[k+1].vitorias){
                Jogador t=v[k];v[k]=v[k+1];v[k+1]=t;
            }
}

void init_tabuleiro(Jogo *j){
    float passo = (float)TAB_SZ / 3.0f;
    float offset = passo / 2.0f - CEL_SZ / 2.0f;
    for(int i=0;i<3;i++)
        for(int k=0;k<3;k++){
            Celula *c=&j->tabuleiro[i][k];  
            c->linha=i; c->coluna=k; c->estado=VAZIO;
            c->pos.x = TAB_X + k*passo + offset; 
            c->pos.y = TAB_Y + i*passo + offset;
        }
    j->turno=JOGADOR_X; j->jogadas=0; j->encerrado=0; j->vencedor=0;
}

int verificar_vencedor(Jogo *j){
    
    CelulaEstado b[3][3];
    for(int i=0;i<3;i++) for(int k=0;k<3;k++) b[i][k]=j->tabuleiro[i][k].estado;
    for(int i=0;i<3;i++){
        if(b[i][0]&&b[i][0]==b[i][1]&&b[i][1]==b[i][2]) return b[i][0];
        if(b[0][i]&&b[0][i]==b[1][i]&&b[1][i]==b[2][i]) return b[0][i];
    }
    if(b[0][0]&&b[0][0]==b[1][1]&&b[1][1]==b[2][2]) return b[0][0];
    if(b[0][2]&&b[0][2]==b[1][1]&&b[1][1]==b[2][0]) return b[0][2];
    if(j->jogadas==9) return 3;
    return 0;
}

void cpu_jogar(Jogo *j){
    int m=-1,n=-1;
    
    for(int i=0;i<3&&m<0;i++) for(int k=0;k<3&&m<0;k++) if(j->tabuleiro[i][k].estado==VAZIO){
        j->tabuleiro[i][k].estado=JOGADOR_O;
        if(verificar_vencedor(j)==JOGADOR_O){m=i;n=k;}
        j->tabuleiro[i][k].estado=VAZIO;
    }
    
    for(int i=0;i<3&&m<0;i++) for(int k=0;k<3&&m<0;k++) if(j->tabuleiro[i][k].estado==VAZIO){
        j->tabuleiro[i][k].estado=JOGADOR_X;
        if(verificar_vencedor(j)==JOGADOR_X){m=i;n=k;}
        j->tabuleiro[i][k].estado=VAZIO;
    }
   
    if(m<0&&j->tabuleiro[1][1].estado==VAZIO){m=1;n=1;}
    
    for(int i=0;i<3&&m<0;i++) for(int k=0;k<3&&m<0;k++) if(j->tabuleiro[i][k].estado==VAZIO){m=i;n=k;}
    if(m>=0) j->tabuleiro[m][n].estado=JOGADOR_O;
}

void registrar_resultado(Jogo *j){
    Jogador *jx=&j->jogadores[0];   
    Jogador *jo=&j->jogadores[1];
    if(j->vencedor==1){jx->vitorias++;jo->derrotas++;}
    else if(j->vencedor==2){jo->vitorias++;jx->derrotas++;}
    else{jx->empates++;jo->empates++;}
    
    if(j->num_partidas>=j->cap_historico){
        j->cap_historico*=2;
        Partida *novo=(Partida*)realloc(j->historico,j->cap_historico*sizeof(Partida));
        if(!novo)return; j->historico=novo;
    }
    Partida *p=&j->historico[j->num_partidas++];
    str_cpy(p->nome_x,jx->nome,sizeof(p->nome_x));
    str_cpy(p->nome_o,jo->nome,sizeof(p->nome_o));
    p->vencedor=j->vencedor==3?0:j->vencedor;
    p->jogadas=j->jogadas; p->timestamp=(long)time(NULL);
    log_partida(p);
    salvar_placar(j);
}

void fazer_jogada(Jogo *j,int lin,int col){
    if(j->encerrado) return;
    Celula *c=&j->tabuleiro[lin][col];   
    if(c->estado!=VAZIO) return;
    c->estado=j->turno; j->jogadas++;
    int res=verificar_vencedor(j);
    if(res){j->encerrado=1;j->vencedor=res;registrar_resultado(j);return;}
    j->turno=(j->turno==JOGADOR_X)?JOGADOR_O:JOGADOR_X;
    if(j->modo==MODO_CPU&&j->turno==JOGADOR_O){
        cpu_jogar(j); j->jogadas++;
        res=verificar_vencedor(j);
        if(res){j->encerrado=1;j->vencedor=res;registrar_resultado(j);return;}
        j->turno=JOGADOR_X;
    }
}

void carregar_assets(Jogo *j){
    j->img_fundo        = al_load_bitmap("assets_velha/fundo.jpg");
    j->img_tabuleiro    = al_load_bitmap("assets_velha/tabuleiro.png");
    j->img_x            = al_load_bitmap("assets_velha/peca_x.png");
    j->img_o            = al_load_bitmap("assets_velha/peca_o.png");
    j->img_pergaminho_v = al_load_bitmap("assets_velha/pergaminho_v.jpg");
    j->img_pergaminho_h = al_load_bitmap("assets_velha/pergaminho_h.jpg");
}

void liberar_assets(Jogo *j){
    if(j->img_fundo)        al_destroy_bitmap(j->img_fundo);
    if(j->img_tabuleiro)    al_destroy_bitmap(j->img_tabuleiro);
    if(j->img_x)            al_destroy_bitmap(j->img_x);
    if(j->img_o)            al_destroy_bitmap(j->img_o);
    if(j->img_pergaminho_v) al_destroy_bitmap(j->img_pergaminho_v);
    if(j->img_pergaminho_h) al_destroy_bitmap(j->img_pergaminho_h);
}


void draw_bmp_fit(ALLEGRO_BITMAP *b,float x,float y,float w,float h){
    if(!b)return;
    al_draw_scaled_bitmap(b,0,0,al_get_bitmap_width(b),al_get_bitmap_height(b),x,y,w,h,0);
}

void draw_bmp_c(ALLEGRO_BITMAP *b,float cx,float cy,float sz){
    if(!b)return;
    int bw=al_get_bitmap_width(b),bh=al_get_bitmap_height(b);
    al_draw_scaled_bitmap(b,0,0,bw,bh,cx-sz/2,cy-sz/2,sz,sz,0);
}

void txt_sombra(float x,float y,int flags,ALLEGRO_COLOR c,const char *s){
    al_draw_text(g_fnt,C_SOMBRA,x+2,y+2,flags,s);
    al_draw_text(g_fnt,c,x,y,flags,s);
}


void desenhar_jogo(Jogo *j){
    
    draw_bmp_fit(j->img_fundo,0,0,W,H);

    al_draw_filled_rectangle(0,0,W,H,al_map_rgba(0,0,0,80));

    draw_bmp_fit(j->img_tabuleiro,TAB_X,TAB_Y,TAB_SZ,TAB_SZ);

    for(int i=0;i<3;i++)
        for(int k=0;k<3;k++){
            Celula *c=&j->tabuleiro[i][k];
            float cx=c->pos.x+CEL_SZ/2.0f;
            float cy=c->pos.y+CEL_SZ/2.0f;
            if(c->estado==JOGADOR_X) draw_bmp_c(j->img_x,cx,cy,CEL_SZ-8);
            else if(c->estado==JOGADOR_O) draw_bmp_c(j->img_o,cx,cy,CEL_SZ-8);
        }

    draw_bmp_fit(j->img_pergaminho_v,630,80,158,440);

    txt_sombra(710,115,ALLEGRO_ALIGN_CENTRE,C_TITULO,"PLACAR");
    char buf[48];
    snprintf(buf,sizeof(buf),"%s",j->jogadores[0].nome);
    txt_sombra(710,155,ALLEGRO_ALIGN_CENTRE,C_TEXTO,buf);
    snprintf(buf,sizeof(buf),"Vit: %d",j->jogadores[0].vitorias);
    txt_sombra(710,175,ALLEGRO_ALIGN_CENTRE,C_CERTO,buf);
    snprintf(buf,sizeof(buf),"Der: %d",j->jogadores[0].derrotas);
    txt_sombra(710,193,ALLEGRO_ALIGN_CENTRE,C_ERRADO,buf);
    snprintf(buf,sizeof(buf),"Emp: %d",j->jogadores[0].empates);
    txt_sombra(710,211,ALLEGRO_ALIGN_CENTRE,C_DICA,buf);

    al_draw_line(660,238,760,238,al_map_rgb(120,80,30),2);

    txt_sombra(710,248,ALLEGRO_ALIGN_CENTRE,C_TEXTO,j->jogadores[1].nome);
    snprintf(buf,sizeof(buf),"Vit: %d",j->jogadores[1].vitorias);
    txt_sombra(710,268,ALLEGRO_ALIGN_CENTRE,C_CERTO,buf);
    snprintf(buf,sizeof(buf),"Der: %d",j->jogadores[1].derrotas);
    txt_sombra(710,286,ALLEGRO_ALIGN_CENTRE,C_ERRADO,buf);
    snprintf(buf,sizeof(buf),"Emp: %d",j->jogadores[1].empates);
    txt_sombra(710,304,ALLEGRO_ALIGN_CENTRE,C_DICA,buf);

    draw_bmp_fit(j->img_pergaminho_h,150,545,500,50);
    if(j->encerrado){
        if(j->vencedor==3)
            txt_sombra(400,553,ALLEGRO_ALIGN_CENTRE,C_TITULO,"EMPATE!");
        else{
            char wb[48]="";
            str_cpy(wb,(j->vencedor==1)?j->jogadores[0].nome:j->jogadores[1].nome,sizeof(wb));
            str_cat(wb," VENCEU!",sizeof(wb));
            txt_sombra(400,553,ALLEGRO_ALIGN_CENTRE,C_CERTO,wb);
        }
        txt_sombra(400,570,ALLEGRO_ALIGN_CENTRE,C_TEXTO,"[R] Novo  [P] Placar  [ESC] Menu");
    } else {
        char *nome=(j->turno==JOGADOR_X)?j->jogadores[0].nome:j->jogadores[1].nome;
        char *simb=(j->turno==JOGADOR_X)?"X":"O";
        char tb[48]="Vez de: "; str_cat(tb,nome,sizeof(tb)); str_cat(tb," (",sizeof(tb)); str_cat(tb,simb,sizeof(tb)); str_cat(tb,")",sizeof(tb));
        txt_sombra(400,555,ALLEGRO_ALIGN_CENTRE,C_TITULO,tb);
        txt_sombra(400,572,ALLEGRO_ALIGN_CENTRE,C_DICA,"[ESC] Menu");
    }

    if(!j->encerrado){
        ALLEGRO_COLOR tc=(j->turno==JOGADOR_X)?C_CERTO:C_ERRADO;
        ALLEGRO_BITMAP *ti=(j->turno==JOGADOR_X)?j->img_x:j->img_o;
        if(ti) draw_bmp_c(ti,W-40,35,50);
        txt_sombra(W-65,40,ALLEGRO_ALIGN_RIGHT,tc,"Jogando:");
    }
}

typedef struct{float x,y,w,h;const char*txt;}Btn;
static Btn bmenu[]={
    {290,340,220,46,"Humano vs Humano"},
    {290,398,220,46,"Humano vs CPU"},
    {290,456,220,46,"Ver Placar"},
    {290,514,220,46,"Sair"}
};
static int g_hov=-1;

int pt_btn(Btn *b,float mx,float my){return mx>=b->x&&mx<=b->x+b->w&&my>=b->y&&my<=b->y+b->h;}

void draw_btn(Btn *b,int hov){
    ALLEGRO_COLOR bg=hov?C_BTN_H:C_BTN;
    al_draw_filled_rounded_rectangle(b->x,b->y,b->x+b->w,b->y+b->h,8,8,bg);
    al_draw_rounded_rectangle(b->x,b->y,b->x+b->w,b->y+b->h,8,8,al_map_rgb(180,130,60),2);
    txt_sombra(b->x+b->w/2,b->y+13,ALLEGRO_ALIGN_CENTRE,C_TITULO,b->txt);
}

void desenhar_menu(Jogo *j){
    draw_bmp_fit(j->img_fundo,0,0,W,H);
    al_draw_filled_rectangle(0,0,W,H,al_map_rgba(0,0,0,140));

    draw_bmp_fit(j->img_pergaminho_h,150,30,500,120);
    txt_sombra(W/2,55,ALLEGRO_ALIGN_CENTRE,C_TITULO,"JOGO DA VELHA");
    txt_sombra(W/2,90,ALLEGRO_ALIGN_CENTRE,C_DICA,"Temática Medieval — Allegro 5");

    draw_bmp_fit(j->img_pergaminho_v,50,160,300,340);

    txt_sombra(200,180,ALLEGRO_ALIGN_CENTRE,C_TITULO,"Jogadores");

    txt_sombra(200,225,ALLEGRO_ALIGN_CENTRE,C_TEXTO,"Jogador X:");
    al_draw_filled_rounded_rectangle(85,245,315,270,5,5,
        j->foco_input==0?al_map_rgba(200,160,80,120):al_map_rgba(80,60,30,120));
    al_draw_rounded_rectangle(85,245,315,270,5,5,al_map_rgb(180,130,60),2);
    char nbuf[28]=""; str_cpy(nbuf,j->input[0],sizeof(nbuf));
    if(j->foco_input==0) str_cat(nbuf,"_",sizeof(nbuf));
    txt_sombra(200,250,ALLEGRO_ALIGN_CENTRE,C_TITULO,nbuf);

    txt_sombra(200,295,ALLEGRO_ALIGN_CENTRE,C_TEXTO,"Jogador O:");
    al_draw_filled_rounded_rectangle(85,315,315,340,5,5,
        j->foco_input==1?al_map_rgba(200,160,80,120):al_map_rgba(80,60,30,120));
    al_draw_rounded_rectangle(85,315,315,340,5,5,al_map_rgb(180,130,60),2);
    char obuf[28]=""; str_cpy(obuf,j->input[1],sizeof(obuf));
    if(j->foco_input==1) str_cat(obuf,"_",sizeof(obuf));
    txt_sombra(200,320,ALLEGRO_ALIGN_CENTRE,C_TITULO,obuf);

    txt_sombra(200,370,ALLEGRO_ALIGN_CENTRE,C_DICA,"[TAB] Alternar campo");
    txt_sombra(200,390,ALLEGRO_ALIGN_CENTRE,C_DICA,"[ENTER] Confirmar nome");

    for(int i=0;i<4;i++) draw_btn(&bmenu[i],g_hov==i);
}

void desenhar_placar(Jogo *j){
    draw_bmp_fit(j->img_fundo,0,0,W,H);
    al_draw_filled_rectangle(0,0,W,H,al_map_rgba(0,0,0,150));

    draw_bmp_fit(j->img_pergaminho_h,150,20,500,80);
    txt_sombra(W/2,40,ALLEGRO_ALIGN_CENTRE,C_TITULO,"PLACAR");

    Jogador *cp=aloc_jogadores(j->num_jogadores);
    if(cp){
        for(int i=0;i<j->num_jogadores;i++) cp[i]=j->jogadores[i];
        ordenar_jogadores(cp,j->num_jogadores);

        draw_bmp_fit(j->img_pergaminho_v,220,110,360,360);
        for(int i=0;i<j->num_jogadores;i++){
            ALLEGRO_COLOR cor=(i==0)?C_TITULO:C_TEXTO;
            char buf[64]; snprintf(buf,sizeof(buf),"%d. %s",i+1,cp[i].nome);
            txt_sombra(400,140+i*90,ALLEGRO_ALIGN_CENTRE,cor,buf);
            snprintf(buf,sizeof(buf),"Vit:%d  Emp:%d  Der:%d",cp[i].vitorias,cp[i].empates,cp[i].derrotas);
            txt_sombra(400,162+i*90,ALLEGRO_ALIGN_CENTRE,C_DICA,buf);
        }
        free(cp);
    }

    draw_bmp_fit(j->img_pergaminho_h,100,480,600,100);
    txt_sombra(W/2,490,ALLEGRO_ALIGN_CENTRE,C_TITULO,"Ultimas Partidas");
    int ini=j->num_partidas-3; if(ini<0)ini=0;
    for(int i=ini;i<j->num_partidas;i++){
        Partida *p=&j->historico[i];
        const char *res=p->vencedor==0?"EMPATE":p->vencedor==1?p->nome_x:p->nome_o;
        char buf[80]; snprintf(buf,sizeof(buf),"%s vs %s — %s",p->nome_x,p->nome_o,res);
        txt_sombra(W/2,512+(i-ini)*18,ALLEGRO_ALIGN_CENTRE,C_TEXTO,buf);
    }
    if(j->num_partidas==0)
        txt_sombra(W/2,515,ALLEGRO_ALIGN_CENTRE,C_DICA,"Nenhuma partida ainda.");

    txt_sombra(W/2,H-20,ALLEGRO_ALIGN_CENTRE,C_DICA,"[ESC] Voltar");
}

Jogo *criar_jogo(void){
    Jogo *j=(Jogo*)malloc(sizeof(Jogo));
    if(!j)return NULL;
    j->num_jogadores=2;
    j->jogadores=aloc_jogadores(2);
    j->historico=aloc_historico(CAP_INI);
    j->num_partidas=0; j->cap_historico=CAP_INI;
    j->tela=TELA_MENU; j->modo=MODO_PVP;
    j->foco_input=0;
    
    str_cpy(j->jogadores[0].nome,"JogadorX",sizeof(j->jogadores[0].nome));
    str_cpy(j->jogadores[1].nome,"JogadorO",sizeof(j->jogadores[1].nome));
    j->jogadores[0].vitorias=j->jogadores[0].derrotas=j->jogadores[0].empates=0;
    j->jogadores[1].vitorias=j->jogadores[1].derrotas=j->jogadores[1].empates=0;
    str_cpy(j->input[0],"JogadorX",sizeof(j->input[0])); j->input_len[0]=8;
    str_cpy(j->input[1],"JogadorO",sizeof(j->input[1])); j->input_len[1]=8;
    
    j->img_fundo=j->img_tabuleiro=j->img_x=j->img_o=j->img_pergaminho_v=j->img_pergaminho_h=NULL;
    init_tabuleiro(j);
    return j;
}

void destruir_jogo(Jogo *j){
    if(!j)return;
    liberar_assets(j);
    free(j->jogadores); free(j->historico); free(j);
}

void iniciar_partida(Jogo *j,ModoJogo modo){
    j->modo=modo;
    
    if(j->input_len[0]>0) str_cpy(j->jogadores[0].nome,j->input[0],sizeof(j->jogadores[0].nome));
    if(j->input_len[1]>0){
        if(modo==MODO_CPU) str_cpy(j->jogadores[1].nome,"CPU",sizeof(j->jogadores[1].nome));
        else str_cpy(j->jogadores[1].nome,j->input[1],sizeof(j->jogadores[1].nome));
    }
    
    carregar_placar(j);
    init_tabuleiro(j);
    j->tela=TELA_JOGO;
}

int main(void){
    srand((unsigned)time(NULL));
    al_init(); al_install_keyboard(); al_install_mouse();
    al_init_font_addon(); al_init_primitives_addon(); al_init_image_addon();

    g_disp=al_create_display(W,H);
    al_set_window_title(g_disp,"Jogo da Velha Medieval");
    g_timer=al_create_timer(1.0/60.0);
    g_fila=al_create_event_queue();
    al_register_event_source(g_fila,al_get_display_event_source(g_disp));
    al_register_event_source(g_fila,al_get_keyboard_event_source());
    al_register_event_source(g_fila,al_get_mouse_event_source());
    al_register_event_source(g_fila,al_get_timer_event_source(g_timer));

    g_fnt=al_create_builtin_font();
    C_TEXTO =al_map_rgb(60,30,10);
    C_TITULO=al_map_rgb(120,60,10);
    C_SOMBRA=al_map_rgba(255,240,200,160);
    C_BTN   =al_map_rgba(80,50,20,200);
    C_BTN_H =al_map_rgba(140,90,30,220);
    C_CERTO =al_map_rgb(30,120,50);
    C_ERRADO=al_map_rgb(160,40,40);
    C_DICA  =al_map_rgb(80,60,30);

    al_start_timer(g_timer);
    Jogo *jogo=criar_jogo();
    carregar_assets(jogo);

    int redraw=1, rodando=1;
    while(rodando){
        ALLEGRO_EVENT ev;
        al_wait_for_event(g_fila,&ev);
        if(ev.type==ALLEGRO_EVENT_DISPLAY_CLOSE){rodando=0;break;}
        if(ev.type==ALLEGRO_EVENT_TIMER){redraw=1;}

        if(ev.type==ALLEGRO_EVENT_MOUSE_AXES&&jogo->tela==TELA_MENU){
            g_hov=-1;
            for(int i=0;i<4;i++) if(pt_btn(&bmenu[i],ev.mouse.x,ev.mouse.y)) g_hov=i;
        }

        if(ev.type==ALLEGRO_EVENT_MOUSE_BUTTON_DOWN&&ev.mouse.button==1){
            float mx=ev.mouse.x,my=ev.mouse.y;
            if(jogo->tela==TELA_MENU){
                if(mx>=85&&mx<=315&&my>=245&&my<=270) jogo->foco_input=0;
                if(mx>=85&&mx<=315&&my>=315&&my<=340) jogo->foco_input=1;
                if(pt_btn(&bmenu[0],mx,my)) iniciar_partida(jogo,MODO_PVP);
                else if(pt_btn(&bmenu[1],mx,my)) iniciar_partida(jogo,MODO_CPU);
                else if(pt_btn(&bmenu[2],mx,my)) jogo->tela=TELA_PLACAR;
                else if(pt_btn(&bmenu[3],mx,my)) rodando=0;
            } else if(jogo->tela==TELA_JOGO&&!jogo->encerrado){
                float passo=(float)TAB_SZ/3.0f;
                int col=(int)((mx-TAB_X)/passo);
                int lin=(int)((my-TAB_Y)/passo);
                if(lin>=0&&lin<3&&col>=0&&col<3&&mx>=TAB_X&&my>=TAB_Y&&mx<=TAB_X+TAB_SZ&&my<=TAB_Y+TAB_SZ)
                    fazer_jogada(jogo,lin,col);
            }
        }
        if(ev.type==ALLEGRO_EVENT_KEY_CHAR){
            int uni=ev.keyboard.unichar, key=ev.keyboard.keycode;
            if(jogo->tela==TELA_MENU){
                int f=jogo->foco_input;
                if(key==ALLEGRO_KEY_TAB) jogo->foco_input=1-f;
                else if(key==ALLEGRO_KEY_BACKSPACE&&jogo->input_len[f]>0)
                    jogo->input[f][--jogo->input_len[f]]='\0';
                else if(key==ALLEGRO_KEY_ENTER&&jogo->input_len[f]>0)
                    jogo->foco_input=1-f;
                else if(((uni>='a'&&uni<='z')||(uni>='A'&&uni<='Z')||uni==' ')&&jogo->input_len[f]<23){
                    jogo->input[f][jogo->input_len[f]++]=(char)(uni>='a'?uni-32:uni);
                    jogo->input[f][jogo->input_len[f]]='\0';
                }
                else if(key==ALLEGRO_KEY_ESCAPE) rodando=0;
            } else if(jogo->tela==TELA_JOGO){
                if(key==ALLEGRO_KEY_R){init_tabuleiro(jogo);}
                else if(key==ALLEGRO_KEY_P){jogo->tela=TELA_PLACAR;}
                else if(key==ALLEGRO_KEY_ESCAPE){jogo->tela=TELA_MENU;}
                int num=-1;
                if(key==ALLEGRO_KEY_1||key==ALLEGRO_KEY_PAD_1)num=0;
                if(key==ALLEGRO_KEY_2||key==ALLEGRO_KEY_PAD_2)num=1;
                if(key==ALLEGRO_KEY_3||key==ALLEGRO_KEY_PAD_3)num=2;
                if(key==ALLEGRO_KEY_4||key==ALLEGRO_KEY_PAD_4)num=3;
                if(key==ALLEGRO_KEY_5||key==ALLEGRO_KEY_PAD_5)num=4;
                if(key==ALLEGRO_KEY_6||key==ALLEGRO_KEY_PAD_6)num=5;
                if(key==ALLEGRO_KEY_7||key==ALLEGRO_KEY_PAD_7)num=6;
                if(key==ALLEGRO_KEY_8||key==ALLEGRO_KEY_PAD_8)num=7;
                if(key==ALLEGRO_KEY_9||key==ALLEGRO_KEY_PAD_9)num=8;
                if(num>=0) fazer_jogada(jogo,num/3,num%3);
            } else if(jogo->tela==TELA_PLACAR){
                if(key==ALLEGRO_KEY_ESCAPE) jogo->tela=TELA_MENU;
            }
        }

        if(redraw&&al_is_event_queue_empty(g_fila)){
            redraw=0;
            switch(jogo->tela){
                case TELA_MENU:   desenhar_menu(jogo);   break;
                case TELA_JOGO:   desenhar_jogo(jogo);   break;
                case TELA_PLACAR: desenhar_placar(jogo); break;
            }
            al_flip_display();
        }
    }

    destruir_jogo(jogo);
    al_destroy_font(g_fnt);
    al_destroy_event_queue(g_fila);
    al_destroy_timer(g_timer);
    al_destroy_display(g_disp);
    return 0;
}
