#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------
 * Estruturas de dados
 * ------------------------------------------------------------ */
typedef struct {
    char nome[30];  /* até 29 + '\0' */
    char cor[10];   /* até 9  + '\0'  (dono/controle) -- armazenada normalizada (minúscula) */
    int  tropas;
} Territorio;

typedef struct {
    char nome[30];  /* rótulo para o jogador (opcional) */
    char cor[10];   /* COR ÚNICA no jogo (normalizada) */
    char *missao;   /* armazenada com malloc */
} Jogador;

/* ------------------------------------------------------------
 * Missões pré-definidas
 *  (A verificação transforma o texto para minúsculas)
 * ------------------------------------------------------------ */
static const char *MISSOES[] = {
    "Conquistar 3 territorios",
    "Eliminar todas as tropas da cor Vermelha",
    "Controlar pelo menos 50% do mapa",
    "Conquistar 2 territorios seguidos",
    "Eliminar todas as tropas da cor Azul"
};
enum { TOTAL_MISSOES = sizeof(MISSOES) / sizeof(MISSOES[0]) };

/* ------------------------------------------------------------
 * Utilidades de normalização e entrada
 * ------------------------------------------------------------ */

/* remove espaços/tabs no início e fim */
static void trim(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    size_t i = 0;
    while (i < len && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) i++;
    size_t j = len;
    while (j > i && (s[j-1] == ' ' || s[j-1] == '\t' || s[j-1] == '\r')) j--;
    if (i > 0) memmove(s, s + i, j - i);
    s[j - i] = '\0';
}

/* converte para minúsculas (apenas ASCII) */
static void to_lower(char *s) {
    if (!s) return;
    for (; *s; s++) {
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
    }
}

/* normaliza token simples: trim + lowercase */
static void normalize_token(char *s) {
    trim(s);
    to_lower(s);
}

/* leitura de linha com prompt */
static void read_line(const char *prompt, char *buf, size_t size) {
    printf("%s", prompt);
    if (!fgets(buf, (int)size, stdin)) {
        buf[0] = '\0';
        return;
    }
    buf[strcspn(buf, "\n")] = '\0';
}

/* leitura de inteiro com validação */
static int read_int(const char *prompt) {
    char line[64];
    int x;
    for (;;) {
        printf("%s", prompt);
        if (!fgets(line, sizeof(line), stdin)) continue;
        if (sscanf(line, "%d", &x) == 1) return x;
        printf("Entrada inválida. Tente novamente.\n");
    }
}

/* ------------------------------------------------------------
 * Dado 1..6
 * ------------------------------------------------------------ */
static int rolar_dado(void) {
    return (rand() % 6) + 1;
}

/* ------------------------------------------------------------
 * Validações de regras pedidas
 * ------------------------------------------------------------ */

/* cores já estão normalizadas; strcmp basta */
static int cores_iguais(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

/* Confere se a cor já foi usada por algum jogador 0..(j-1). */
static int cor_ja_usada(Jogador *players, int j, const char *cor) {
    for (int i = 0; i < j; i++) {
        if (cores_iguais(players[i].cor, cor)) return 1;
    }
    return 0;
}

/* Verifica se já existe território com mesmo NOME para a MESMA COR.
   (Restrições: não pode ter territórios iguais por jogador) */
static int territorio_duplicado_para_cor(const Territorio *mapa, int n,
                                         const char *nome, const char *cor) {
    for (int i = 0; i < n; i++) {
        if (strcmp(mapa[i].cor, cor) == 0 && strcmp(mapa[i].nome, nome) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Verifica se uma cor pertence a algum jogador. */
static int cor_pertence_a_jogador(const Jogador *players, int P, const char *cor) {
    for (int i = 0; i < P; i++) if (strcmp(players[i].cor, cor) == 0) return 1;
    return 0;
}

/* Retorna índice do jogador dono da cor, ou -1. */
static int idx_jogador_por_cor(const Jogador *players, int P, const char *cor) {
    for (int i = 0; i < P; i++) if (strcmp(players[i].cor, cor) == 0) return i;
    return -1;
}

/* ------------------------------------------------------------
 * Cadastro de jogadores (cores únicas + nome)
 * ------------------------------------------------------------ */
static void cadastrar_jogadores(Jogador *players, int P) {
    printf("\n=============================================\n");
    printf("         CADASTRO DE JOGADORES (%d)\n", P);
    printf("=============================================\n");

    for (int i = 0; i < P; i++) {
        printf("\n>>> Jogador %d de %d\n", i + 1, P);
        read_line("Nome do jogador: ", players[i].nome, sizeof(players[i].nome));

        for (;;) {
            read_line("Cor do exército (única no jogo): ", players[i].cor, sizeof(players[i].cor));
            normalize_token(players[i].cor);  /* NORMALIZA A COR! */
            if (players[i].cor[0] == '\0') {
                printf("Cor não pode ser vazia.\n");
                continue;
            }
            if (cor_ja_usada(players, i, players[i].cor)) {
                printf("Essa cor já foi escolhida por outro jogador. Escolha outra.\n");
                continue;
            }
            break;
        }
        players[i].missao = NULL; /* será atribuída depois */
    }
}

/* ------------------------------------------------------------
 * Cadastro dos territórios
 *   - n territórios (dinâmico)
 *   - cada território deve usar uma cor de jogador válida
 *   - não permitir nome repetido para a MESMA cor
 * ------------------------------------------------------------ */
static void cadastrar_territorios(const Jogador *players, int P, Territorio *mapa, int n) {
    printf("\n=============================================\n");
    printf("       CADASTRO DE TERRITÓRIOS (%d)\n", n);
    printf("=============================================\n");
    printf("Cores disponíveis dos jogadores:\n");
    for (int i = 0; i < P; i++) {
        printf("  - %s (%s)\n", players[i].nome, players[i].cor);
    }

    for (int i = 0; i < n; i++) {
        printf("\n>>> Território %d de %d\n", i + 1, n);

        /* Nome do território */
        for (;;) {
            read_line("Nome do território: ", mapa[i].nome, sizeof(mapa[i].nome));
            if (mapa[i].nome[0] == '\0') {
                printf("Nome não pode ser vazio.\n");
                continue;
            }
            break;
        }

        /* Cor (dono) — deve ser uma das cores cadastradas de jogadores */
        for (;;) {
            read_line("Cor do exército (deve ser uma cor de jogador): ", mapa[i].cor, sizeof(mapa[i].cor));
            normalize_token(mapa[i].cor);  /* NORMALIZA A COR! */
            if (!cor_pertence_a_jogador(players, P, mapa[i].cor)) {
                printf("Cor inválida. Use uma das cores dos jogadores.\n");
                continue;
            }
            /* Checa duplicidade de território para este dono (mesma cor) */
            if (territorio_duplicado_para_cor(mapa, i, mapa[i].nome, mapa[i].cor)) {
                printf("Já existe um território com esse nome para esse jogador. Escolha outro nome ou dono.\n");
                continue;
            }
            break;
        }

        /* Tropas (>=0) */
        int t = read_int("Número de tropas: ");
        if (t < 0) t = 0;
        mapa[i].tropas = t;
    }
}

/* ------------------------------------------------------------
 * Impressão do mapa
 * ------------------------------------------------------------ */
static void imprimir_mapa(const Territorio *mapa, int n) {
    printf("\n=============================================\n");
    printf("                 ESTADO DO MAPA\n");
    printf("=============================================\n");
    printf("Idx | %-28s | %-8s | Tropas\n", "Nome", "Cor");
    printf("----+------------------------------+----------+--------\n");
    for (int i = 0; i < n; i++) {
        printf("%3d | %-28s | %-8s | %6d\n",
               i + 1, mapa[i].nome, mapa[i].cor, mapa[i].tropas);
    }
    printf("-------------------------------------------------------\n");
}

/* ------------------------------------------------------------
 * Atribuição e exibição de missão
 *   - Missão sorteada e copiada para destino com malloc/strcpy
 * ------------------------------------------------------------ */
static void atribuirMissao(char **destino, const char *missoes[], int total) {
    if (!destino || total <= 0) return;
    int idx = rand() % total;
    const char *src = missoes[idx];
    size_t len = strlen(src) + 1;

    char *buf = (char *)malloc(len);
    if (!buf) {
        perror("malloc missão");
        return;
    }
    strcpy(buf, src);
    *destino = buf;
}

static void exibirMissao(const Jogador *j) {
    if (!j || !j->missao) return;
    printf("\nMissão do jogador %s [%s]: %s\n", j->nome, j->cor, j->missao);
}

/* ------------------------------------------------------------
 * Ataque
 *   - validações: índices, cores diferentes, >=2 tropas
 *   - regra pedida
 * ------------------------------------------------------------ */
static void atacar(Territorio *atacante, Territorio *defensor) {
    if (!atacante || !defensor) return;
    if (atacante == defensor) {
        printf("Você não pode atacar o mesmo território.\n");
        return;
    }
    if (strcmp(atacante->cor, defensor->cor) == 0) {
        printf("Ataque inválido: as cores são iguais (%s). Ataque apenas inimigos.\n", atacante->cor);
        return;
    }
    if (atacante->tropas < 2) {
        printf("Ataque inválido: o atacante precisa ter ao menos 2 tropas.\n");
        return;
    }

    int dado_atk = rolar_dado();
    int dado_def = rolar_dado();
    printf("Rolagem -> Atacante: %d | Defensor: %d\n", dado_atk, dado_def);

    if (dado_atk > dado_def) {
        int mover = atacante->tropas / 2;   /* metade das tropas */
        if (mover < 1) mover = 1;

        /* Conquista / mudança de dono + reposicionamento */
        strncpy(defensor->cor, atacante->cor, sizeof(defensor->cor));
        defensor->cor[sizeof(defensor->cor) - 1] = '\0';

        atacante->tropas -= mover;
        defensor->tropas  = mover;

        printf("Atacante venceu! %d tropas moveram para \"%s\". Nova cor: %s\n",
               mover, defensor->nome, defensor->cor);
    } else {
        if (atacante->tropas > 0) atacante->tropas -= 1;
        if (dado_atk == dado_def) {
            printf("Empate! O atacante perde 1 tropa.\n");
        } else {
            printf("Defensor venceu! O atacante perde 1 tropa.\n");
        }
    }
}

/* ------------------------------------------------------------
 * Verificação de missão (lógica simples via strstr em lowercase)
 * ------------------------------------------------------------ */
static int contar_territorios_da_cor(const Territorio *mapa, int n, const char *cor) {
    int c = 0;
    for (int i = 0; i < n; i++) if (strcmp(mapa[i].cor, cor) == 0) c++;
    return c;
}

static int existem_tropas_da_cor(const Territorio *mapa, int n, const char *cor) {
    for (int i = 0; i < n; i++)
        if (strcmp(mapa[i].cor, cor) == 0 && mapa[i].tropas > 0)
            return 1;
    return 0;
}

static int verificarMissao(const char *missao, const Jogador *j, const Territorio *mapa, int n) {
    if (!missao || !j || !mapa) return 0;

    /* cria uma cópia lowercase da missão para matching robusto */
    size_t len = strlen(missao) + 1;
    char *m = (char *)malloc(len);
    if (!m) return 0;
    strcpy(m, missao);
    to_lower(m);

    /* Conquistar 3 territorios */
    if (strstr(m, "conquistar 3 territorios")) {
        free(m);
        return contar_territorios_da_cor(mapa, n, j->cor) >= 3;
    }

    /* Eliminar todas as tropas da cor vermelha */
    if (strstr(m, "eliminar todas as tropas da cor vermelha")) {
        free(m);
        return !existem_tropas_da_cor(mapa, n, "vermelha"); /* comparando com lowercase */
    }

    /* Eliminar todas as tropas da cor azul */
    if (strstr(m, "eliminar todas as tropas da cor azul")) {
        free(m);
        return !existem_tropas_da_cor(mapa, n, "azul");
    }

    /* Controlar pelo menos 50% do mapa */
    if (strstr(m, "controlar pelo menos 50% do mapa")) {
        free(m);
        int meus = contar_territorios_da_cor(mapa, n, j->cor);
        return (meus * 2) >= n;  /* meus >= n/2 arredondado pra cima */
    }

    /* Conquistar 2 territorios seguidos (simplificado: possuir >=2) */
    if (strstr(m, "conquistar 2 territorios seguidos")) {
        free(m);
        return contar_territorios_da_cor(mapa, n, j->cor) >= 2;
    }

    free(m);
    /* Missão não reconhecida -> não cumpre */
    return 0;
}

/* ------------------------------------------------------------
 * Menu
 * ------------------------------------------------------------ */
static int menu(void) {
    printf("\n=============================================\n");
    printf("                 MENU PRINCIPAL\n");
    printf("=============================================\n");
    printf("1 - Atacar\n");
    printf("2 - Exibir mapa\n");
    printf("0 - Sair\n");
    return read_int("Escolha: ");
}

/* ------------------------------------------------------------
 * Liberação de memória
 *   - libera vetor de territórios e missões dos jogadores
 * ------------------------------------------------------------ */
static void liberarMemoria(Territorio *mapa, Jogador *players, int P) {
    if (mapa) free(mapa);
    if (players) {
        for (int i = 0; i < P; i++) {
            free(players[i].missao);
            players[i].missao = NULL;
        }
    }
}

/* ------------------------------------------------------------
 * main
 * ------------------------------------------------------------ */
int main(void) {
    srand((unsigned)time(NULL));

    /* Quantidade de jogadores (mínimo 2) e territórios (mínimo 2) */
    int P;
    do {
        P = read_int("Quantos jogadores? (min 2): ");
        if (P < 2) printf("São necessários pelo menos 2 jogadores.\n");
    } while (P < 2);

    int n;
    do {
        n = read_int("Quantos territórios haverá no mapa? (min 2): ");
        if (n < 2) printf("É necessário pelo menos 2 territórios.\n");
    } while (n < 2);

    /* Aloca jogadores e mapa */
    Jogador *players = (Jogador *)calloc((size_t)P, sizeof(Jogador));
    Territorio *mapa = (Territorio *)calloc((size_t)n, sizeof(Territorio));
    if (!players || !mapa) {
        perror("calloc");
        free(players);
        free(mapa);
        return 1;
    }

    /* Cadastro jogadores com cores únicas (normalizadas) */
    cadastrar_jogadores(players, P);

    /* Atribui missão a cada jogador e exibe (uma vez) */
    for (int i = 0; i < P; i++) {
        atribuirMissao(&players[i].missao, MISSOES, TOTAL_MISSOES);
        exibirMissao(&players[i]);
    }

    /* Cadastro de territórios com validações de dono/duplicidade */
    cadastrar_territorios(players, P, mapa, n);
    imprimir_mapa(mapa, n);

    /* Loop do jogo */
    for (;;) {
        int op = menu();
        if (op == 0) {
            break;
        } else if (op == 2) {
            imprimir_mapa(mapa, n);
        } else if (op == 1) {
            /* Mostra quem é quem antes do ataque */
            printf("\nJogadores:\n");
            for (int i = 0; i < P; i++) {
                printf("  %d) %s [%s]\n", i + 1, players[i].nome, players[i].cor);
            }
            int jsel = read_int("Escolha o jogador atacante (1..P): ");
            if (jsel < 1 || jsel > P) {
                printf("Jogador inválido.\n");
                continue;
            }
            Jogador *J = &players[jsel - 1];

            printf("\nSelecione territórios por índice (1..%d)\n", n);
            int i_atk = read_int("Índice do TERRITÓRIO ATACANTE: ");
            int i_def = read_int("Índice do TERRITÓRIO DEFENSOR: ");

            if (i_atk < 1 || i_atk > n || i_def < 1 || i_def > n) {
                printf("Índices inválidos. Use valores entre 1 e %d.\n", n);
                continue;
            }

            Territorio *atk = &mapa[i_atk - 1];
            Territorio *def = &mapa[i_def - 1];

            /* O território atacante precisa realmente ser do jogador escolhido */
            if (strcmp(atk->cor, J->cor) != 0) {
                printf("Território atacante não pertence ao jogador %s [%s].\n", J->nome, J->cor);
                continue;
            }

            printf("\nAtaque: \"%s\" (%s, %d tropas)  ->  \"%s\" (%s, %d tropas)\n",
                   atk->nome, atk->cor, atk->tropas,
                   def->nome, def->cor, def->tropas);

            atacar(atk, def);
            imprimir_mapa(mapa, n);

            /* Verifica vitória de algum jogador após o turno */
            for (int i = 0; i < P; i++) {
                if (verificarMissao(players[i].missao, &players[i], mapa, n)) {
                    printf("\n>>> VITÓRIA! Jogador %s [%s] cumpriu a missão: %s\n",
                           players[i].nome, players[i].cor, players[i].missao);
                    liberarMemoria(mapa, players, P);
                    free(players);
                    return 0;
                }
            }
        } else {
            printf("Opção inválida.\n");
        }
    }

    liberarMemoria(mapa, players, P);
    free(players);
    printf("Encerrado. Memória liberada com sucesso.\n");
    return 0;
}
