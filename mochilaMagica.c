/*
 * Universidade Presbiteriana Mackenzie
 * Projeto e Analise de Algoritmos II - Projeto 2
 *
 * Jogo de Aventura - Mochila Fracionaria (Estrategia Gulosa)
 *
 * Integrantes do grupo:
 *   - [Integrante 1]
 *   - [Integrante 2]
 *   - [Integrante 3]
 *
 * Compilacao: gcc mochilaMagica.c -o mochilaMagica
 * Execucao:   ./mochilaMagica entrada_jogo.txt saida_jogo.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_FASES   10
#define MAX_ITENS   100
#define MAX_NOME    100
#define MAX_TIPO    30
#define MAX_REGRA   50
#define MAX_LINHA   512
#define EPS         1e-9

/* Identificadores das regras especiais de cada fase. */
typedef enum {
    REGRA_DESCONHECIDA,
    MAGICOS_VALOR_DOBRADO,
    TECNOLOGICOS_INTEIROS,
    SOBREVIVENCIA_DESVALORIZADA,
    TRES_MELHORES_VALOR_PESO
} Regra;

/* Representa um item disponivel em uma fase. */
typedef struct {
    char   nome[MAX_NOME];
    double peso;          /* peso original em kg */
    double valor;         /* valor ajustado conforme a regra da fase */
    char   tipo[MAX_TIPO];
    int    indivisivel;   /* 1 = nao pode ser fracionado */
} Item;

/* Representa uma fase do jogo. */
typedef struct {
    char   nome[MAX_NOME];
    double capacidade;
    char   regra_id[MAX_REGRA];
    Regra  regra;
    Item   itens[MAX_ITENS];
    int    n_itens;
} Fase;

/* =========================================================
 *                   Funcoes utilitarias
 * ========================================================= */

/* Remove espacos em branco no inicio e no fim da string (in-place). */
static void trim(char *s) {
    size_t inicio = 0, fim;
    while (s[inicio] && isspace((unsigned char)s[inicio])) inicio++;
    if (inicio > 0) memmove(s, s + inicio, strlen(s + inicio) + 1);
    fim = strlen(s);
    while (fim > 0 && isspace((unsigned char)s[fim - 1])) {
        s[--fim] = '\0';
    }
}

/* Converte o identificador textual da regra para o enum correspondente. */
static Regra identificar_regra(const char *id) {
    if (strcmp(id, "MAGICOS_VALOR_DOBRADO") == 0)        return MAGICOS_VALOR_DOBRADO;
    if (strcmp(id, "TECNOLOGICOS_INTEIROS") == 0)        return TECNOLOGICOS_INTEIROS;
    if (strcmp(id, "SOBREVIVENCIA_DESVALORIZADA") == 0)  return SOBREVIVENCIA_DESVALORIZADA;
    if (strcmp(id, "TRES_MELHORES_VALOR_PESO") == 0)     return TRES_MELHORES_VALOR_PESO;
    return REGRA_DESCONHECIDA;
}

/* Descricao humana da regra, usada na saida. */
static const char *descricao_regra(Regra r) {
    switch (r) {
        case MAGICOS_VALOR_DOBRADO:       return "Itens magicos com valor dobrado";
        case TECNOLOGICOS_INTEIROS:       return "Itens tecnologicos nao podem ser fracionados";
        case SOBREVIVENCIA_DESVALORIZADA: return "Itens de sobrevivencia perdem 20% do valor";
        case TRES_MELHORES_VALOR_PESO:    return "Apenas os tres itens com maior valor/peso podem ser escolhidos";
        default:                          return "Regra desconhecida";
    }
}

/* =========================================================
 *           Ordenacao manual (selection sort)
 * Sem uso de qsort, conforme exigencia do enunciado.
 * ========================================================= */

static void trocar_itens(Item *a, Item *b) {
    Item tmp = *a;
    *a = *b;
    *b = tmp;
}

/* Ordena itens em ordem decrescente pela razao valor/peso. */
static void ordenar_por_valor_peso(Item *itens, int n) {
    int i, j, idxMax;
    double rMax, r;
    for (i = 0; i < n - 1; i++) {
        idxMax = i;
        rMax = itens[i].valor / itens[i].peso;
        for (j = i + 1; j < n; j++) {
            r = itens[j].valor / itens[j].peso;
            if (r > rMax) {
                rMax = r;
                idxMax = j;
            }
        }
        if (idxMax != i) trocar_itens(&itens[i], &itens[idxMax]);
    }
}

/* =========================================================
 *                Leitura do arquivo de entrada
 * ========================================================= */

/* Faz o parse de uma linha "ITEM: nome, peso, valor, tipo". Retorna 1 em sucesso. */
static int parse_item(char *conteudo, Item *it) {
    char *campos[4];
    int   i, n_campos = 0;
    char *p = conteudo;

    /* Quebra a string em ate 4 campos separados por virgula. */
    campos[n_campos++] = p;
    while (*p && n_campos < 4) {
        if (*p == ',') {
            *p = '\0';
            campos[n_campos++] = p + 1;
        }
        p++;
    }
    if (n_campos < 4) return 0;

    for (i = 0; i < 4; i++) trim(campos[i]);

    strncpy(it->nome, campos[0], MAX_NOME - 1);
    it->nome[MAX_NOME - 1] = '\0';
    it->peso  = atof(campos[1]);
    it->valor = atof(campos[2]);
    strncpy(it->tipo, campos[3], MAX_TIPO - 1);
    it->tipo[MAX_TIPO - 1] = '\0';
    it->indivisivel = 0;

    if (it->peso <= 0.0 || it->valor < 0.0) return 0;
    return 1;
}

/* Le todas as fases do arquivo. Retorna o numero de fases lidas ou -1 em erro. */
static int ler_arquivo(FILE *fin, Fase fases[], int max_fases) {
    char linha[MAX_LINHA];
    int  n = -1;            /* indice da fase atual */
    int  num_linha = 0;

    while (fgets(linha, sizeof(linha), fin)) {
        num_linha++;
        trim(linha);
        if (linha[0] == '\0') continue;

        if (strncmp(linha, "FASE:", 5) == 0) {
            n++;
            if (n >= max_fases) {
                fprintf(stderr, "Erro: numero maximo de fases (%d) excedido.\n", max_fases);
                return -1;
            }
            fases[n].n_itens = 0;
            fases[n].capacidade = 0.0;
            fases[n].regra = REGRA_DESCONHECIDA;
            fases[n].regra_id[0] = '\0';

            char *p = linha + 5;
            trim(p);
            strncpy(fases[n].nome, p, MAX_NOME - 1);
            fases[n].nome[MAX_NOME - 1] = '\0';
        }
        else if (n < 0) {
            fprintf(stderr, "Erro na linha %d: conteudo antes da primeira FASE.\n", num_linha);
            return -1;
        }
        else if (strncmp(linha, "CAPACIDADE:", 11) == 0) {
            if (sscanf(linha + 11, "%lf", &fases[n].capacidade) != 1
                || fases[n].capacidade <= 0.0) {
                fprintf(stderr, "Erro na linha %d: capacidade invalida.\n", num_linha);
                return -1;
            }
        }
        else if (strncmp(linha, "REGRA:", 6) == 0) {
            char *p = linha + 6;
            trim(p);
            strncpy(fases[n].regra_id, p, MAX_REGRA - 1);
            fases[n].regra_id[MAX_REGRA - 1] = '\0';
            fases[n].regra = identificar_regra(fases[n].regra_id);
            if (fases[n].regra == REGRA_DESCONHECIDA) {
                fprintf(stderr, "Aviso na linha %d: regra '%s' desconhecida.\n",
                        num_linha, fases[n].regra_id);
            }
        }
        else if (strncmp(linha, "ITEM:", 5) == 0) {
            if (fases[n].n_itens >= MAX_ITENS) {
                fprintf(stderr, "Erro: maximo de itens (%d) excedido na fase '%s'.\n",
                        MAX_ITENS, fases[n].nome);
                return -1;
            }
            char *p = linha + 5;
            trim(p);
            if (!parse_item(p, &fases[n].itens[fases[n].n_itens])) {
                fprintf(stderr, "Erro na linha %d: item mal formatado.\n", num_linha);
                return -1;
            }
            fases[n].n_itens++;
        }
        /* Linhas desconhecidas sao ignoradas silenciosamente. */
    }

    return n + 1;
}

/* =========================================================
 *               Aplicacao das regras especiais
 * ========================================================= */

/* Modifica os itens da fase conforme a regra especial. */
static void aplicar_regra(Fase *fase) {
    int i;
    switch (fase->regra) {
        case MAGICOS_VALOR_DOBRADO:
            for (i = 0; i < fase->n_itens; i++) {
                if (strcmp(fase->itens[i].tipo, "magico") == 0) {
                    fase->itens[i].valor *= 2.0;
                }
            }
            break;

        case TECNOLOGICOS_INTEIROS:
            for (i = 0; i < fase->n_itens; i++) {
                if (strcmp(fase->itens[i].tipo, "tecnologico") == 0) {
                    fase->itens[i].indivisivel = 1;
                }
            }
            break;

        case SOBREVIVENCIA_DESVALORIZADA:
            for (i = 0; i < fase->n_itens; i++) {
                if (strcmp(fase->itens[i].tipo, "sobrevivencia") == 0) {
                    fase->itens[i].valor *= 0.8;
                }
            }
            break;

        case TRES_MELHORES_VALOR_PESO:
            ordenar_por_valor_peso(fase->itens, fase->n_itens);
            if (fase->n_itens > 3) fase->n_itens = 3;
            break;

        default:
            break;
    }
}

/* =========================================================
 *           Estrategia gulosa da mochila fracionaria
 * ========================================================= */

/*
 * Executa o guloso na fase e escreve os itens escolhidos no arquivo de saida.
 * Retorna o lucro obtido na fase.
 */
static double processar_fase(FILE *fout, Fase *fase) {
    int    i;
    double cap_restante = fase->capacidade;
    double lucro_fase   = 0.0;

    fprintf(fout, "--- FASE: %s ---\n", fase->nome);
    fprintf(fout, "Capacidade da mochila: %.2f kg\n", fase->capacidade);
    fprintf(fout, "Regra aplicada: %s\n\n", descricao_regra(fase->regra));

    ordenar_por_valor_peso(fase->itens, fase->n_itens);

    for (i = 0; i < fase->n_itens; i++) {
        Item *it = &fase->itens[i];
        if (cap_restante <= EPS) break;

        if (it->peso <= cap_restante + EPS) {
            /* Cabe inteiro na mochila. */
            fprintf(fout, "Pegou (inteiro) %s (%.2fkg, R$ %.2f)\n",
                    it->nome, it->peso, it->valor);
            lucro_fase   += it->valor;
            cap_restante -= it->peso;
        }
        else {
            /* Nao cabe inteiro. */
            if (it->indivisivel) {
                /* Regra impede fracionar - pula e tenta os proximos. */
                continue;
            }
            double fracao = cap_restante / it->peso;
            double valor_frac = it->valor * fracao;
            fprintf(fout, "Pegou (fracionado) %s (%.2fkg, R$ %.2f)\n",
                    it->nome, cap_restante, valor_frac);
            lucro_fase   += valor_frac;
            cap_restante  = 0.0;
        }
    }

    fprintf(fout, "Lucro da fase: R$ %.2f\n\n", lucro_fase);
    return lucro_fase;
}

/* =========================================================
 *                          Main
 * ========================================================= */

int main(int argc, char *argv[]) {
    static Fase fases[MAX_FASES];   /* static para evitar grande alocacao no stack */
    int    n_fases, i;
    double lucro_total = 0.0;
    FILE  *fin, *fout;

    if (argc < 3) {
        fprintf(stderr, "Uso: %s <arquivo_entrada.txt> <arquivo_saida.txt>\n", argv[0]);
        return 1;
    }

    fin = fopen(argv[1], "r");
    if (!fin) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo de entrada '%s'.\n", argv[1]);
        return 1;
    }

    fout = fopen(argv[2], "w");
    if (!fout) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo de saida '%s'.\n", argv[2]);
        fclose(fin);
        return 1;
    }

    n_fases = ler_arquivo(fin, fases, MAX_FASES);
    fclose(fin);

    if (n_fases <= 0) {
        fprintf(stderr, "Erro: nenhuma fase valida foi lida do arquivo de entrada.\n");
        fclose(fout);
        return 1;
    }

    for (i = 0; i < n_fases; i++) {
        aplicar_regra(&fases[i]);
        lucro_total += processar_fase(fout, &fases[i]);
    }

    fprintf(fout, "=========================================\n");
    fprintf(fout, "Lucro total acumulado: R$ %.2f\n", lucro_total);

    fclose(fout);
    printf("Processamento concluido. Saida gravada em '%s'.\n", argv[2]);
    return 0;
}
