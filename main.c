#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HASH_SIZE 100003 // Número primo para reduzir colisões na Tabela Hash
#define MAX_LINE 2048    // Tamanho máximo para leitura de uma linha
#define MAX_PESQUISADORES 150000

/* ============================================================================
 * ESTRUTURAS DE DADOS
 * ============================================================================
 */

// 1. Tabela Hash para localizar o nome (Nome -> Id)
typedef struct HashNome {
  char *nome;
  int id;
  struct HashNome *prox;
} HashNome;

// 2. Tabela Hash para Títulos (Título -> Lista de IDs dos Autores)
typedef struct ListaAutores {
  int idAutor;
  struct ListaAutores *prox;
} ListaAutores;

typedef struct HashTitulo {
  char *titulo;
  ListaAutores *autores;
  struct HashTitulo *prox;
} HashTitulo;

// 3. Estruturas para o Grafo de Colaborações (Lista de Adjacência)
typedef struct ListaTitulos {
  char *titulo;
  struct ListaTitulos *prox;
} ListaTitulos;

typedef struct Aresta {
  int idDestino;
  ListaTitulos *titulos; // Trabalhos publicados em colaboração
  struct Aresta *prox;
} Aresta;

// struct de agrupamento das estruturas usadas, serve para diminuir a
// quantidade de argumentos passados para as funções
typedef struct {
  HashNome *tabelaNomes[HASH_SIZE];
  HashTitulo *tabelaTitulos[HASH_SIZE];
  char *idParaNome[MAX_PESQUISADORES]; // Índice: localiza o nome a partir do nó
  Aresta *grafo[MAX_PESQUISADORES];    // Grafo: liga os pesquisadores
  int totalPesquisadores;
} contexto;

/* ============================================================================
 * FUNÇÕES AUXILIARES E DE HASH
 * ============================================================================
 */

// Função de hash djb2 (muito eficiente para strings)
unsigned int calcularHash(const char *str) {
  unsigned long hash = 5381;
  int c;

  if (str == NULL)
    exit(1);

  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash % HASH_SIZE;
}

// Remove quebras de linha do final da string
void trim_str(char *str) { str[strcspn(str, "\r\n")] = 0; }

// Cria e inicializa o sistema
contexto *criar_contexto() {
  contexto *s = (contexto *)malloc(sizeof(contexto));

  if (s == NULL)
    exit(1);

  s->totalPesquisadores = 0;

  for (int i = 0; i < HASH_SIZE; i++) {
    s->tabelaNomes[i] = NULL;
    s->tabelaTitulos[i] = NULL;
  }

  for (int i = 0; i < MAX_PESQUISADORES; i++) {
    s->idParaNome[i] = NULL;
    s->grafo[i] = NULL;
  }

  return s;
}

/* ============================================================================
 * GERENCIAMENTO DAS TABELAS HASH
 * ============================================================================
 */

// Insere um pesquisador na Hash de Nomes (ou retorna o ID se já existir)
int obterOuInserirPesquisador(contexto *s, const char *nome) {
  unsigned int indice = calcularHash(nome);
  HashNome *atual = s->tabelaNomes[indice];

  // Busca na lista encadeada da Hash (Tratamento de colisão)
  while (atual != NULL) {
    if (strcmp(atual->nome, nome) == 0) {
      return atual->id; // Pesquisador já existe
    }
    atual = atual->prox;
  }

  if (s->totalPesquisadores >= MAX_PESQUISADORES) {
    printf("ERRO: limite maximo de %d pesquisadores atingido!\n",
           MAX_PESQUISADORES);
    exit(1);
  }

  // Se não existir, insere um novo
  s->totalPesquisadores++;
  int novoId = s->totalPesquisadores;
  HashNome *novoNo = (HashNome *)malloc(sizeof(HashNome));

  if (novoNo == NULL)
    exit(1);

  novoNo->nome = strdup(nome);
  novoNo->id = novoId;
  novoNo->prox = s->tabelaNomes[indice];
  s->tabelaNomes[indice] = novoNo;

  // Atualiza o índice direto (ID -> Nome)
  s->idParaNome[novoId] = strdup(nome);

  return novoId;
}

// Insere um autor em um título na Hash de Títulos
void inserirTitulo(contexto *s, const char *titulo, int idAutor) {
  unsigned int indice = calcularHash(titulo);
  HashTitulo *atual = s->tabelaTitulos[indice];

  // Procura o título (Colisão Tradicional)
  while (atual != NULL) {
    if (strcmp(atual->titulo, titulo) == 0) {
      // Título encontrado. Verifica se o autor já está na lista
      ListaAutores *autorAtual = atual->autores;
      while (autorAtual != NULL) {
        if (autorAtual->idAutor == idAutor)
          return; // Autor já registrado neste título
        autorAtual = autorAtual->prox;
      }
      // Colisão Repetida: Adiciona o novo autor à lista deste título
      ListaAutores *novoAutor = (ListaAutores *)malloc(sizeof(ListaAutores));

      if (novoAutor == NULL)
        exit(1);

      novoAutor->idAutor = idAutor;
      novoAutor->prox = atual->autores;
      atual->autores = novoAutor;
      return;
    }
    atual = atual->prox;
  }

  // Título não existe, cria um novo nó na tabela
  HashTitulo *novoTitulo = (HashTitulo *)malloc(sizeof(HashTitulo));

  if (novoTitulo == NULL)
    exit(1);

  novoTitulo->titulo = strdup(titulo);

  ListaAutores *novoAutor = (ListaAutores *)malloc(sizeof(ListaAutores));

  if (novoAutor == NULL)
    exit(1);

  novoAutor->idAutor = idAutor;
  novoAutor->prox = NULL;

  novoTitulo->autores = novoAutor;
  novoTitulo->prox = s->tabelaTitulos[indice];
  s->tabelaTitulos[indice] = novoTitulo;
}

/* ============================================================================
 * CONSTRUÇÃO DO GRAFO
 * ============================================================================
 */

// Adiciona uma aresta direcionada u -> v com o título do trabalho
void adicionarAresta(contexto *s, int u, int v, const char *titulo) {
  if (u == v)
    return; // Evita auto-loop (um autor colaborando com ele mesmo)

  Aresta *atual = s->grafo[u];
  // Verifica se os dois pesquisadores já possuem uma aresta (já colaboraram
  // antes)
  while (atual != NULL) {
    if (atual->idDestino == v) {
      // Aresta já existe. Adiciona o título à lista de colaborações da aresta
      ListaTitulos *novoTitulo = (ListaTitulos *)malloc(sizeof(ListaTitulos));

      if (novoTitulo == NULL)
        exit(1);

      novoTitulo->titulo = strdup(titulo);
      novoTitulo->prox = atual->titulos;
      atual->titulos = novoTitulo;
      return;
    }
    atual = atual->prox;
  }

  // Se não existir, cria a aresta entre os pesquisadores
  Aresta *novaAresta = (Aresta *)malloc(sizeof(Aresta));

  if (novaAresta == NULL)
    exit(1);

  novaAresta->idDestino = v;

  ListaTitulos *novoTitulo = (ListaTitulos *)malloc(sizeof(ListaTitulos));

  if (novoTitulo == NULL)
    exit(1);

  novoTitulo->titulo = strdup(titulo);
  novoTitulo->prox = NULL;

  novaAresta->titulos = novoTitulo;
  novaAresta->prox = s->grafo[u];
  s->grafo[u] = novaAresta;
}

// Constrói o grafo iterando sobre todos os títulos e ligando seus co-autores
void construirGrafo(contexto *s) {
  for (int i = 0; i < HASH_SIZE; i++) {
    HashTitulo *tituloAtual = s->tabelaTitulos[i];
    while (tituloAtual != NULL) {

      // Para cada par de autores no mesmo trabalho, cria uma aresta (ida e
      // volta)
      ListaAutores *autorA = tituloAtual->autores;
      while (autorA != NULL) {
        ListaAutores *autorB = autorA->prox;
        while (autorB != NULL) {
          adicionarAresta(s, autorA->idAutor, autorB->idAutor,
                          tituloAtual->titulo);
          adicionarAresta(s, autorB->idAutor, autorA->idAutor,
                          tituloAtual->titulo);
          autorB = autorB->prox;
        }
        autorA = autorA->prox;
      }
      tituloAtual = tituloAtual->prox;
    }
  }
}

/* ============================================================================
 * OPERAÇÕES
 * ============================================================================
 */

// Operação 1:
// Dado um nome, listar os nomes dos seus colaboradores (com quem ele está
// conectado no grafo).
void listarColaboradores(contexto *s, const char *nomeBusca) {
  unsigned int indice = calcularHash(nomeBusca);
  HashNome *atual = s->tabelaNomes[indice];
  int idPesquisador = -1;

  // Resolve conflitos da tabela hash
  while (atual != NULL) {
    if (strcmp(atual->nome, nomeBusca) == 0) {
      idPesquisador = atual->id;
      break;
    }
    atual = atual->prox;
  }

  if (idPesquisador == -1) {
    printf("\n[-] Pesquisador '%s' nao encontrado.\n", nomeBusca);
    return;
  }

  printf("\n--- Colaboradores de %s ---\n", nomeBusca);
  Aresta *aresta = s->grafo[idPesquisador];
  if (aresta == NULL) {
    printf("Nenhum colaborador encontrado.\n");
    return;
  }

  while (aresta != NULL) {
    printf("> %s\n", s->idParaNome[aresta->idDestino]);

    aresta = aresta->prox;
  }
}

// Operação 2:
// Dado o título de um trabalho, listar os nomes de todos os autores
void listarAutoresTrabalho(contexto *s, const char *tituloBusca) {
  unsigned int indice = calcularHash(tituloBusca);
  HashTitulo *atual = s->tabelaTitulos[indice];

  while (atual != NULL) {
    if (strcmp(atual->titulo, tituloBusca) == 0) {
      printf("\n--- Autores de '%s' ---\n", tituloBusca);

      ListaAutores *autor = atual->autores;
      while (autor != NULL) {
        printf("> %s\n", s->idParaNome[autor->idAutor]);
        autor = autor->prox;
      }
      return;
    }
    atual = atual->prox;
  }
  printf("\n[-] Trabalho '%s' nao encontrado.\n", tituloBusca);
}

// Operação 3:
// Calcular o maior grau de um vértice do grafo
void calcularMaiorGrau(contexto *s) {
  int maiorGrau = -1;
  int idMaiorGrau = -1;

  for (int i = 0; i < s->totalPesquisadores; i++) {
    int grauAtual = 0;
    Aresta *aresta = s->grafo[i];

    while (aresta != NULL) {
      grauAtual++;
      aresta = aresta->prox;
    }

    if (grauAtual > maiorGrau) {
      maiorGrau = grauAtual;
      idMaiorGrau = i;
    }
  }

  if (idMaiorGrau != -1) {
    printf("\n--- Maior Grau do Grafo ---\n");
    printf("Pesquisador: %s\n", s->idParaNome[idMaiorGrau]);
    printf("Grau: %d\n", maiorGrau);
  } else {
    printf("\n[-] O grafo esta vazio.\n");
  }
}

// Operação 4:
// Calcular o grau médio do grafo
void calcularGrauMedio(contexto *s) {
  if (s->totalPesquisadores == 0) {
    printf("\n[-] O grafo esta vazio.\n");
    return;
  }

  int somaGraus = 0;

  for (int i = 0; i < s->totalPesquisadores; i++) {
    Aresta *aresta = s->grafo[i];
    while (aresta != NULL) {
      somaGraus++;
      aresta = aresta->prox;
    }
  }

  double grauMedio = (double)somaGraus / s->totalPesquisadores;
  printf("\n--- Grau Medio do Grafo ---\n");
  printf("Grau Medio: %.2f\n", grauMedio);
}

/* ============================================================================
 * LEITURA DO ARQUIVO
 * ============================================================================
 */

void carregarDados(contexto *s, const char *nomeArquivo) {
  FILE *file = fopen(nomeArquivo, "r");
  if (!file) {
    printf("Erro: Nao foi possivel abrir o arquivo '%s'.\n", nomeArquivo);
    exit(1);
  }

  char linha[MAX_LINE];
  while (fgets(linha, sizeof(linha), file)) {
    trim_str(linha);

    if (strlen(linha) == 0)
      continue;

    // Procura pelo separador '\t'
    char *tabPtr = strchr(linha, '\t');

    if (tabPtr != NULL) {
      *tabPtr = '\0'; // Separa a string em duas quebrando no tab

      char *nome = linha;
      char *titulo = tabPtr + 1;

      // printf("Pesquisador: %s\n", nome);
      // printf("Título: %s\n", titulo);

      // Insere nas Hashs
      int idAutor = obterOuInserirPesquisador(s, nome);
      inserirTitulo(s, titulo, idAutor);
    }
  }
  fclose(file);

  // Com os dados salvos no contexto, constrói as arestas do grafo
  construirGrafo(s);
}

/* ============================================================================
 * MAIN
 * ============================================================================
 */

void print_header() {
  printf("\n=========================================\n");
  printf("           MENU DE COLABORACOES          \n");
  printf("=========================================\n");
  printf("1. Listar colaboradores de um pesquisador\n");
  printf("2. Listar autores de um trabalho\n");
  printf("3. Calcular maior grau de um vertice\n");
  printf("4. Calcular grau medio do grafo\n");
  printf("5. Sair\n");
  printf("Escolha uma opcao: ");
}

void loop_options(contexto *s) {
  int opcao = 0;
  char buffer[MAX_LINE];

  while (opcao != 5) {
    print_header();

    scanf("%d", &opcao);
    getchar();

    printf("\n");

    switch (opcao) {
    case 1:
      printf("Digite o nome do pesquisador: ");

      fgets(buffer, sizeof(buffer), stdin);
      trim_str(buffer);

      listarColaboradores(s, buffer);
      break;
    case 2:
      printf("Digite o titulo do trabalho: ");

      fgets(buffer, sizeof(buffer), stdin);
      trim_str(buffer);

      listarAutoresTrabalho(s, buffer);
      break;
    case 3:
      calcularMaiorGrau(s);
      break;
    case 4:
      calcularGrauMedio(s);
      break;
    case 5:
      printf("Finalizando\n");
      return;
    default:
      printf("Opção inválida!\n");
      break;
    }

    sleep(1);
  }
}

int main() {
  contexto *s = criar_contexto();

  printf("Carregando dados\n");
  carregarDados(s, "dadosPesquisadores.txt");
  printf("Dados carregados com sucesso!\n");

  loop_options(s);

  return 0;
}
