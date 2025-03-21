#ifndef KILO_H
#define KILO_H

/* ESTRUTURAS */

struct estado_editor 
{
	struct termios terminal_backup;
	struct construtor_string *construtor;
	int linhas;
	int colunas;
};

// construtores são basicamente buffers que crescem automaticamente.
#define FATOR_CRESCIMENTO 2
struct construtor_string
{
	char *buffer;
	size_t tamanho;
	size_t capacidade;
};

/* ESTRUTURAS */

/* UTILIDADE */
void
fatal (const char *msg);

struct construtor_string
*criar_construtor_string (size_t capacidade);

void
destruir_construtor_string (struct construtor_string **construtor);

int
adicionar_caractere_construtor_string (struct construtor_string *construtor, char caractere);

int 
adicionar_string_construtor_string (struct construtor_string *construtor, const char *string, char separador);

char
*construir_string_construtor_string (struct construtor_string *construtor);

void
limpar_construtor_string (struct construtor_string *construtor);

/* FIM UTILIDADE*/

/* TERMINAL */
void 
copia_terminal (struct termios *alvo, struct termios *fonte);

void
ativa_modo_canonico (struct termios *alvo);

void 
desativa_modo_canonico (void);

char
ler_tecla (void);

void
pegar_posicao_do_cursor (int *vertical, int *horizontal);

void
pegar_total_linhas_colunas (void);

void
salvar_posicao_do_cursor (void);

void
restaurar_posicao_do_cursor (void);

void
desenhar_til (void);

void
atutalizar_tela_do_editor (void);

void
limpar_tela (void);

/* FIM TERMINAL */

/* ENTRADA  */

void
mapear_tecla_para_acao_do_editor (void);

/* FIM ENTRADA  */

/* INICIALIZAÇÂO  */

void
inicializar_editor (void);

/* FIM INICIALIZAÇÂO  */
#endif
