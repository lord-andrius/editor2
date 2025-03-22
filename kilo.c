#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include "kilo.h"

#define CTRL_KEY(q) ((q) & 0x1f)

static struct estado_editor estado_editor;


/* UTILIDADE */
void
fatal (const char *msg)
{
	const char *sequencia_de_escape_que_limpa_a_tela = "\033[2J";
	write (STDOUT_FILENO, sequencia_de_escape_que_limpa_a_tela, strlen (sequencia_de_escape_que_limpa_a_tela));
	const char *sequencia_de_escape_que_reposiciona_o_cursor = "\033[1;1H";
	write (STDOUT_FILENO, sequencia_de_escape_que_reposiciona_o_cursor, strlen (sequencia_de_escape_que_reposiciona_o_cursor));
	perror (msg);
	exit (EXIT_FAILURE);
}

struct construtor_string
*criar_construtor_string (size_t capacidade)
{
	struct construtor_string *construtor = malloc (sizeof (struct construtor_string));
	if (construtor == NULL)
	{
		return NULL;
	}

	char *buffer = calloc (capacidade, sizeof (char));

	if (buffer == NULL)
	{
		free (construtor);
		return NULL;
	}

	construtor->buffer = buffer;
	construtor->tamanho = 0;
	construtor->capacidade = capacidade;

	return construtor;
}

void
destruir_construtor_string (struct construtor_string **construtor)
{
	if ((*construtor) != NULL)
	{
		if ((*construtor)->buffer != NULL)
		{
			free((*construtor)->buffer);
		}
		free(*construtor);
	}
	else
	{
		fatal ("Isso é um bug alguém tentou dar free num construtor nulo");
	}

}

// se ainda tiver capacidade retorna 0
// se precisar aumentar e a operação de aumentar for bem sucedida retorna 1 
// capacidade. Se a operação de aumentar falhar retorna -1
// NOTA: essa função só deve ser usada pela funções do construtor_string
int
aumentar_buffer_construtor_string (struct construtor_string *construtor)
{
	if (construtor == NULL)
	{
		return -1;
	}

	if (construtor->tamanho + 1 < construtor->capacidade)
	{
		return 0;
	}

	if (construtor->capacidade < 1)
	{
		construtor->capacidade = 1;
	}

	char *novo_buffer = realloc (construtor->buffer, construtor->capacidade * FATOR_CRESCIMENTO);

	if (novo_buffer == NULL)
	{
		return -1;
	}

	construtor->buffer = novo_buffer;
	construtor->capacidade *= FATOR_CRESCIMENTO;

	return 1;
}

int
aumentar_buffer_para_acomodar_n_construtor_string (struct construtor_string *construtor, size_t tamanho_em_bytes)
{
	if (construtor == NULL)
	{
		return -1;
	}	

	if (construtor->tamanho + tamanho_em_bytes  + 1 < construtor->capacidade)
	{
		return 0;
	}

	if (construtor->capacidade < 1)
	{
		construtor->capacidade = 1;
	}

	size_t nova_capacidade = construtor->capacidade;
	for (; nova_capacidade < tamanho_em_bytes + construtor->tamanho; nova_capacidade *= FATOR_CRESCIMENTO);

	char *novo_buffer = realloc (construtor->buffer, nova_capacidade);

	if (novo_buffer == NULL)
	{
		return -1;
	}

	construtor->buffer = novo_buffer;
	construtor->capacidade = nova_capacidade;

	return 1;
}

// retorna 0 em caso de sucesso
int
adicionar_caractere_construtor_string (struct construtor_string *construtor, char caractere)
{
	if (aumentar_buffer_construtor_string (construtor) < 0)
	{
		return -1;
	}	
	
	construtor->buffer[construtor->tamanho++] = caractere;

	return 0;

}


// o separador é opcional
// ele é o que separa a string que está sendo colocada agora
// e a última string colocada
// NOTA: se o separador é '\0' ele é ignorado
int 
adicionar_string_construtor_string (struct construtor_string *construtor, const char *string, char separador)
{
	if (construtor == NULL)
	{
		return -1;
	}
	size_t tamanho_da_string = strlen (string);
	int retorno_aumentar_buffer;
	if (separador == '\0')
	{
		retorno_aumentar_buffer = aumentar_buffer_para_acomodar_n_construtor_string(
			construtor,
			tamanho_da_string
		);
	}
	else
	{
		retorno_aumentar_buffer = aumentar_buffer_para_acomodar_n_construtor_string(
			construtor,
			tamanho_da_string
		);

	}

	if (retorno_aumentar_buffer < 0)
	{
		return -1;
	}

	char *posicao_no_buffer = &construtor->buffer[construtor->tamanho];

	if (separador != '\0')
	{
		*posicao_no_buffer++ = separador;
		construtor->tamanho++;
	}	

	memcpy (posicao_no_buffer, string, tamanho_da_string);
	construtor->tamanho += tamanho_da_string;
	return 0;
}

char
*construir_string_construtor_string (struct construtor_string *construtor)
{
	if (construtor == NULL)
	{
		return NULL;
	}

	if (construtor->tamanho == 0)
	{
		return NULL;
	}

	// +1 por causa do NULL
	char *string_feita = malloc (construtor->tamanho + 1);
	
	if (string_feita == NULL)
	{
		return NULL;
	}

	memcpy (string_feita, construtor->buffer, construtor->tamanho);

	string_feita[construtor->tamanho] = '\0';

	return string_feita;

}

void
limpar_construtor_string (struct construtor_string *construtor)
{
	construtor->tamanho = 0;
}


/* FIM UTILIDADE */

/* TERMINAL */
void copia_terminal (struct termios *alvo, struct termios *fonte)
{
	alvo->c_iflag = fonte->c_iflag;
	alvo->c_oflag = fonte->c_oflag;
	alvo->c_cflag = fonte->c_cflag;
	alvo->c_lflag = fonte->c_lflag;
	for (int i = 0; i < NCCS; i++)
		alvo->c_cc[i] = fonte->c_cc[i];
}

void 
ativa_modo_canonico (struct termios *terminal)
{
	if (tcgetattr (STDIN_FILENO, terminal) != 0)
	{
		fatal ("Falha não foi possível pegar os atributos do termianl");
	}
	
	copia_terminal (&estado_editor.terminal_backup, terminal);
	terminal->c_lflag ^= (ECHO | ICANON | ISIG | IEXTEN);
	terminal->c_iflag ^= (IXOFF | IXON | ICRNL | BRKINT | INPCK | ISTRIP);
	terminal->c_oflag ^= (OPOST);
	terminal->c_cflag |= (CS8);
	terminal->c_cc[VTIME] = 1;
	terminal->c_cc[VMIN] = 0;

	

	if (tcsetattr (STDIN_FILENO, TCSANOW, terminal) != 0)
	{
		fatal ("Falha não foi possível setar o atributo");	
	}
}

void
desativa_modo_canonico (void)
{
	if (tcsetattr (STDIN_FILENO, TCSAFLUSH, &estado_editor.terminal_backup) != 0)
	{
		fatal ("Falha não foi possível setar o atributo");	
	}
}


char
ler_tecla (void)
{
	int bytes_lidos;
	char tecla;
	while ((bytes_lidos = read (STDIN_FILENO, &tecla, 1)) != 1) {
		if (bytes_lidos == -1 && errno != EAGAIN) fatal ("erro ao tentar ler tecla do terminal");
	}
	return tecla;
}

void
pegar_posicao_do_cursor (int *vertical, int *horizontal)
{
	const char *sequencia_de_escape_que_pega_posicao_do_cursor = "\033[6n";
	write (STDOUT_FILENO, sequencia_de_escape_que_pega_posicao_do_cursor, strlen (sequencia_de_escape_que_pega_posicao_do_cursor));
	char buffer[255] = {0};
	char *posicao_atual_no_buffer = buffer;
	do
	{
		read (STDIN_FILENO, posicao_atual_no_buffer, 1);
		// se eu não achei ainda o 'R' e não tem mais espaço no buffer
		if (&buffer[sizeof (buffer) - 1] == posicao_atual_no_buffer && *posicao_atual_no_buffer != 'R')
		{
			fatal ("Posição do buffer é muito grande");
		}
	} while (*(posicao_atual_no_buffer++) != 'R');
	// a resposta dada do é no seguinte formato ^[<v>;<h>R
	char *comeco_posicao_vertical = strchr (buffer, '[') + 1;
	char *fim_posicao_vertical = strchr (buffer, ';') - 1;
	char *comeco_posicao_horizontal = strchr (buffer, ';') + 1;
	char *fim_posicao_horizontal = strchr (buffer, 'R') - 1;
	char buffer_posicao[255] = {0};
	size_t indice_no_buffer_posicao = 0;
	for (char *i = comeco_posicao_vertical; i <= fim_posicao_vertical; i++)
	{
		buffer_posicao[indice_no_buffer_posicao++] = *i; } char *ponteiro_para_caractere_invalido = NULL;
	*vertical = (int)strtol (buffer_posicao, &ponteiro_para_caractere_invalido, 10);
	if (*ponteiro_para_caractere_invalido != '\0')
	{
		fatal ("Posição do cursor não é válida");
	}
	indice_no_buffer_posicao = 0;
	for (char *i = comeco_posicao_horizontal; i <= fim_posicao_horizontal; i++)
	{
		buffer_posicao[indice_no_buffer_posicao++] = *i;
	}
	buffer_posicao[indice_no_buffer_posicao] = '\0';
	*horizontal = (int)strtol (buffer_posicao, &ponteiro_para_caractere_invalido, 10);
	if (*ponteiro_para_caractere_invalido != '\0')
	{
		fatal ("Posição do cursor não é válida");
	}
	return;
}

void
pegar_total_linhas_colunas (void)
{
	struct winsize tamanho_da_janela;
	if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &tamanho_da_janela) != 0)
	{
		// já que o ioctl falhou vamos tentar de outro jeito
		// vamos usar CUD e CUF com o argumento de 999 para irmos pro
		// final da tela então vamos pegar a posicao do cursor. No final
		// voltamos o cursor para a posição inicial
		salvar_posicao_do_cursor (NULL);
		const char *sequencia_de_escape_para_pegar_ultima_pos = "\033[999B\033[999C";
		write (
				STDOUT_FILENO,
			       	sequencia_de_escape_para_pegar_ultima_pos,
				strlen (sequencia_de_escape_para_pegar_ultima_pos)
		);
		pegar_posicao_do_cursor (
				&estado_editor.linhas, 
				&estado_editor.colunas
		);
		restaurar_posicao_do_cursor (NULL);
		if (estado_editor.linhas == -1 || estado_editor.colunas == -1)
			fatal ("Não foi possível pegar as dimensões da tela");
		
	}
	estado_editor.linhas = tamanho_da_janela.ws_row;
	estado_editor.colunas = tamanho_da_janela.ws_col;
}


// Se for o construtor for nulo a operação acontece imediatamente
void
salvar_posicao_do_cursor (struct construtor_string *construtor)
{
   
	const char *sequencia_de_escape_que_salva_a_posicao_do_cursor = "\0337";
  if (construtor == NULL)
  {
    write (STDOUT_FILENO, sequencia_de_escape_que_salva_a_posicao_do_cursor, strlen (sequencia_de_escape_que_salva_a_posicao_do_cursor));
  }
  else
  {
    adicionar_string_construtor_string (construtor, sequencia_de_escape_que_salva_a_posicao_do_cursor, '\0'); 
  }
}

void
restaurar_posicao_do_cursor (struct construtor_string *construtor)
{
	const char *sequencia_de_escape_que_restaura_a_posicao_do_cursor = "\0338";
  if (construtor == NULL)
  {
    write (STDOUT_FILENO, sequencia_de_escape_que_restaura_a_posicao_do_cursor, strlen (sequencia_de_escape_que_restaura_a_posicao_do_cursor));
  }
  else
  {
    adicionar_string_construtor_string (construtor, sequencia_de_escape_que_restaura_a_posicao_do_cursor, '\0');
  }
}

//Essa função precisa de um construtor
void
desenhar_til (struct construtor_string *construtor)
{
	salvar_posicao_do_cursor (construtor);
	const char *sequencia_de_escape_que_move_uma_linha = "\033[1B";
	const char *sequencia_de_escape_que_move_uma_coluna = "\033[1D";
	for (int i = 0; i < estado_editor.linhas; i++)
	{
		adicionar_string_construtor_string (construtor, sequencia_de_escape_que_move_uma_linha, '\0');
		adicionar_string_construtor_string  (construtor, sequencia_de_escape_que_move_uma_coluna, '\0');
		adicionar_caractere_construtor_string (construtor, '~');
	}
	write (STDOUT_FILENO, estado_editor.construtor->buffer, construtor->tamanho);
	restaurar_posicao_do_cursor (construtor);
	
}

// Se o construtor for nulo a funçao vai chamar o write
// Se o construtor não for nulo a sequencia de escape e
// adicionada ao construtor
void
esconder_cursor (struct construtor_string *construtor)
{
  // NOTA: olhar a essa sequência de escape e os modos do vt-100
  const char *sequencia_esconde_cursor = "\033[?25l"; 
  if (construtor == NULL)
  {
    write (STDOUT_FILENO, sequencia_esconde_cursor, strlen(sequencia_esconde_cursor));
  }
  else
  {
    adicionar_string_construtor_string (construtor, sequencia_esconde_cursor,'\0');
  }
}


//Essa função faz com que o cursor escondido
//pela função esconder_cursos volte para a tela
void
restaurar_cursor (struct construtor_string *construtor)
{
  //NOTA: olhar essa sequência de escape e os modos do vt-100 
  const char *sequencia_restaura_cursor = "\033[?25h"; 
  if (construtor == NULL)
  {
    write (STDOUT_FILENO, sequencia_restaura_cursor, strlen(sequencia_restaura_cursor));
  }
  else
  {
    adicionar_string_construtor_string (construtor, sequencia_restaura_cursor, '\0');
  }
}

void
atualizar_tela_do_editor (void)
{
  esconder_cursor (estado_editor.construtor);
	limpar_tela (estado_editor.construtor);
	desenhar_til (estado_editor.construtor);
  restaurar_cursor (estado_editor.construtor); 
  write (STDOUT_FILENO, estado_editor.construtor->buffer, estado_editor.construtor->tamanho);
	limpar_construtor_string (estado_editor.construtor);
}

// Se o construtor for nulo a operação é feita na hora
void
limpar_tela (struct construtor_string *construtor)
{
  const char *sequencia_de_escape_que_limpa_a_tela = "\033[2J";
  const char *sequencia_de_escape_que_reposiciona_o_cursor = "\033[1;1H";
  if (construtor == NULL)
  {
    write (STDOUT_FILENO, sequencia_de_escape_que_limpa_a_tela, strlen (sequencia_de_escape_que_limpa_a_tela));
    write (STDOUT_FILENO, sequencia_de_escape_que_reposiciona_o_cursor, strlen (sequencia_de_escape_que_reposiciona_o_cursor));
  }
  else
  {
    adicionar_string_construtor_string (construtor, sequencia_de_escape_que_limpa_a_tela, '\0');
    adicionar_string_construtor_string (construtor, sequencia_de_escape_que_reposiciona_o_cursor, '\0');
  }
}

/* FIM TERMINAL */

/* ENTRADA */
void
mapear_tecla_para_acao_do_editor (void)
{
	char tecla = ler_tecla ();
	switch (tecla)
	{
		case CTRL_KEY('q'):
			limpar_tela (NULL);
			exit (EXIT_SUCCESS);
			break;

	}
}
/* FIM ENTRADA */

/* INICIALIZAÇÃO */
void inicializar_editor (void)
{
	estado_editor.construtor = criar_construtor_string (256);
	pegar_total_linhas_colunas ();
}
/* FIM INICIALIZAÇÃO */

/* FINALIZAÇÂO */
void desinicializar_editor (void)
{
	destruir_construtor_string (&estado_editor.construtor);
}
/* FIM FINALIZAÇÂO */



int
main (void)
{
	struct termios terminal;

	ativa_modo_canonico (&terminal);
	atexit (desativa_modo_canonico);
	atexit (desinicializar_editor);
	inicializar_editor ();
	while (1)
	{
		mapear_tecla_para_acao_do_editor ();
		atualizar_tela_do_editor ();
	}
	
	return EXIT_SUCCESS;
}
