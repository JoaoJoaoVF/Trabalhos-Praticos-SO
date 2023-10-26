# PAGINADOR DE MEMÓRIA - RELATÓRIO

1. Termo de compromisso

Os membros do grupo afirmam que todo o código desenvolvido para este
trabalho é de autoria própria.  Exceto pelo material listado no item
3 deste relatório, os membros do grupo afirmam não ter copiado
material da Internet nem ter obtido código de terceiros.

2. Membros do grupo e alocação de esforço

Preencha as linhas abaixo com o nome e o e-mail dos integrantes do
grupo.  Substitua marcadores `XX` pela contribuição de cada membro
do grupo no desenvolvimento do trabalho (os valores devem somar
100%).

  * João Vitor Ferreira ferreirajoao@dcc.ufmg.br 33%
  * Lucas Roberto Santos Avelar lucasavelar@dcc.ufmg.br 33%
  * Luiza de Melo Gomes luizademelo@dcc.ufmg.br 34%

3. Referências bibliográficas
  Documentação da biblioteca ucontext.h: https://pubs.opengroup.org/onlinepubs/7908799/xsh/ucontext.h.html
  Documentação da biblioteca signal.h: https://pubs.opengroup.org/onlinepubs/009695399/basedefs/signal.h.html
  Seção "Implementation": https://tildesites.bowdoin.edu/~sbarker/teaching/courses/os/18spring/p3.php
  Create Your Thread Library: https://nitish712.blogspot.com/2012/10/thread-library-using-context-switching.html
  Silverchatz, Galvin, Gagne; Operating System Fundaments, 8th Edition - Capítulo 4: Threads

4. Estruturas de dados

  1. Descreva e justifique as estruturas de dados utilizadas para
     gerência das threads de espaço do usuário (partes 1, 2 e 5).
      
      **dccthread_t**: estrutura principal do trabalho prático, responsável por
      armazenar os atributos que serão manipulados nas funções implementadas
        - __char name[DCCTHREAD_MAX_NAME_SIZE]__: atributo que dá nome à thread.
          Necessário em funções como dccthread_create, que recebem como um dos
          parâmtros a nomenclatura que será dada para cada thread criada.
        - __ucontext_t context__: principal atributo da estrutura, que fornece o
          contexto da thread. Necessária para guardar a relação (link) da thread
          com outras, o tamanho da pilha da thread, a posição inicial da pilha
          na memória e as flags da pilha.
        - __char stack[THREAD_STACK_SIZE] nome__: declara a pilha da thread.
          Será utilizada na inicialização dos valores relacionados à pilha
          dentro do context da thread.
        - __dccthread_t *waiting_for__: atributo que aponta por qual thread a
          atual está esperando, caso exista. Importante principalmente na função
          ddcthread_wait para definir qual thread deve-se esperar.
        - __int has_waited__: atributo tipo flag que indica se a thread já
          passou por dccthread_wait. Utilizada para implementação de um dos
          desafios de ponto extra (dccthread_nexited), em que é necessário
          verificar quantas threads finalizaram sem passar por dccthread_wait.

      **dccthread_timer**: estrutura auxiliar responsável por manter atributos
      importantes para controle de quanto tempo uma thread irá dormir,
      necessária para a função dccthread_sleep.
        - __timer_t timer_id__: atributo para o timer em si. Necessário para que 
          seja criado o temporizador que controla o tempo de sono da thread, incluindo
          variáveis como a ação a ser tomada após fim do tempo (acordar a thread), para
          qual thread o timer aponta e qual o tempo de sono.
        - __struct timespec sleep_time__: atributo para armazenar quanto tempo a thread
          deve permanecer dormindo. Necessário pois será atribuído aos membros de timer_id
          relacionados à tempo de sono em segundos e nanosegundos.
      
  2. Descreva o mecanismo utilizado para sincronizar chamadas de
     dccthread_yield e disparos do temporizador (parte 4).

      Como descrito na especificação do trabalho prático, é necessário desabilitar o temporizador
      momentaneamente após a chamada de algumas funções, que incluem, além de dccthread_yield, 
      dccthread_create, exit, wait e sleep. Desse modo, ao entrar na função dccthread_yield 
      (ou em alguma das outras citadas), é realizada a chamada __sigprocmask(SIG_BLOCK, &mask, NULL)__,
      que bloqueia o sinal do timer enquanto a função realiza seus procedimentos. A função yield, então,
      remove a thread atual da CPU, recolocando-a na lista de threads prontas, chama a thread manager para
      que ela realiza o escalonamento e, só então, desbloqueia o sinal do temporizador chamando
      __sigprocmask(SIG_UNBLOCK, &mask, NULL)__, permitindo que o sinal volte a funcionar normalmente
      enquanto não houver outra chamada de função que precise interremper o disparo do temporizador
      novamente.

5. Extras
    - Implementação da função dccthread_nwaiting para saber quantas threads estão esperando
    - Implementação da função dccthread_nexiting para saber quantas threads finalizaram, mas que não foram alvo de dccthread_wait 