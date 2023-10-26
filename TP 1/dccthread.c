#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <signal.h>
#include "dccthread.h"
#include "dlist.h"
#include <stdio.h>

#define NSEC_VALUE 10000000
#define SEC_VALUE 0

// struct para representar uma thread
typedef struct dccthread
{
    char name[DCCTHREAD_MAX_NAME_SIZE]; // nome da thread
    ucontext_t context;                 // contexto da thread
    char stack[THREAD_STACK_SIZE];      // tamanho da pilha da thread e posição inicial na memória
    dccthread_t *waiting_for;           // thread pela qual esta thread está esperando
    int has_waited;                     // flag que indica se a thread já passou por dccthread_wait()
} dccthread_t;

// struct auxiliar para definir um timer no momento de colocar threads em modo sleep
typedef struct
{
    timer_t timer_id;                   // atributo para ciração do timer em si
    struct timespec sleep_time;         // especificação de quanto tempo a thread deve dormir
} dccthread_timer;

dccthread_t *manager_thread;            // thread gerente para fazer o escalonamento das threads criadas
dccthread_t *main_thread;               // thread principal (corrente) que irá executar a função "func"

struct dlist *waiting;                  // lista de threads em espera
struct dlist *ready;                    // lista de thread prontas para execução
struct dlist *finished;                 // lista de threads terminadas (útil para dccthread_nexited)

// variáveis de suporte à preempção
timer_t timer;
struct sigevent signalevent;
struct itimerspec timerspec;
struct sigaction action;

// variáveis de suporte à colocar threads em modo sleep
dccthread_timer timer_sleep;
struct sigevent signalevent_sleep;
struct itimerspec timerspec_sleep;
struct sigaction action_sleep;

// máscara de sinais bloqueados
sigset_t mask;
sigset_t mask_sleep;

// funcionalidade extra: verificar quantas threads estão esperando por outras threads
int dccthread_nwaiting()
{
    int size = 0;
    struct dnode *current_thread = waiting->head;
    // percorre a lista de threads esperando, incremento o tamanho a cada iteração
    while (current_thread != NULL)
    {
        current_thread = current_thread->next;
        size++;
    }
    return size;
}

// funcionalidade extra: verificar quantas threads finalizaram sem passar por dccthread_wait
int dccthread_nexited(void)
{
    int count = 0;
    struct dnode *current_thread = finished->head;
    // percorre a lista de threads já finalizadas
    while (current_thread != NULL)
    {
        dccthread_t *data = (dccthread_t *)current_thread->data;
        // se a thread indicar que não passou por dccthread_wait, incrementa o contador
        if (data->has_waited == 0)
        {
            count++;
        }
        current_thread = current_thread->next;
    }
    return count;
}

// função auxiliar para verificar se uma thread está presente em uma lista (ambos passados como parâmetro)
int is_thread_in_list(struct dlist *thread_list, dccthread_t *thread)
{
    if (thread_list->count == 0)
        return 0;

    struct dnode *current_thread_item = thread_list->head;
    while (current_thread_item != NULL && (dccthread_t *)current_thread_item->data != thread)
    {
        current_thread_item = current_thread_item->next;
    }

    if (current_thread_item != NULL)
    {
        return 1;
    }

    return 0;
}

// função auxiliar para ser passada como parâmetro para o handler do sigaction 
// (o atributo deve receber uma função com int como parâmetro)
void dccthread_preemption(int _)
{
    dccthread_yield();
}

// função auxiliar que inicializa os atributos das variáveis de suporte à preempção declaradas anteriormente
// cria o temporizador de acordo com essas variáveis
void timer_init()
{
    action.sa_flags = 0;
    action.sa_handler = dccthread_preemption;
    sigaction(SIGRTMIN, &action, NULL);

    signalevent.sigev_notify = SIGEV_SIGNAL;
    signalevent.sigev_signo = SIGRTMIN;
    signalevent.sigev_value.sival_ptr = &timer;

    timer_create(CLOCK_PROCESS_CPUTIME_ID, &signalevent, &timer);

    timerspec.it_value.tv_sec = SEC_VALUE;
    timerspec.it_value.tv_nsec = NSEC_VALUE;
    timerspec.it_interval.tv_sec = SEC_VALUE;
    timerspec.it_interval.tv_nsec = NSEC_VALUE;

    timer_settime(timer, 0, &timerspec, NULL);
}

// inicializa os atributos das máscaras de sinal bloqueado declaradas anteriormente
void mask_init()
{
    sigemptyset(&mask);
    sigaddset(&mask, SIGRTMIN);
    sigaddset(&mask, SIGRTMAX);

    sigemptyset(&mask_sleep);
    sigaddset(&mask_sleep, SIGRTMAX);
}

// função auxiliar para encapsular as atribuições que devem ser realizadas na thread manager
// aloca o espaço da thread na memória e inicializa seu nome e variáveis de contexto
void manager_init(void)
{
    manager_thread = (dccthread_t *)malloc(sizeof(dccthread_t));
    strcpy(manager_thread->name, "manager_thread");
    manager_thread->context.uc_link = NULL;
    manager_thread->context.uc_stack.ss_sp = manager_thread->stack;
    manager_thread->context.uc_stack.ss_size = THREAD_STACK_SIZE;
    manager_thread->context.uc_stack.ss_flags = 0;
    manager_thread->context.uc_sigmask = mask;
    getcontext(&manager_thread->context);
}

void dccthread_init(void (*func)(int), int param)
{
    waiting = dlist_create();
    ready = dlist_create();
    finished = dlist_create();

    mask_init();
    timer_init();
    manager_init();

    main_thread = dccthread_create("main", func, param);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    while (!dlist_empty(ready) || !dlist_empty(waiting))
    {
        sigprocmask(SIG_UNBLOCK, &mask_sleep, NULL);
        sigprocmask(SIG_BLOCK, &mask_sleep, NULL);

        main_thread = (dccthread_t *)dlist_pop_left(ready);

        if (main_thread->waiting_for != NULL)
        {
            if (is_thread_in_list(ready, main_thread->waiting_for) || is_thread_in_list(waiting, main_thread->waiting_for))
            {
                dlist_push_right(ready, main_thread);
                continue;
            }
            else
            {
                main_thread->waiting_for = NULL;
            }
        }
        swapcontext(&manager_thread->context, &main_thread->context);
    }

    dlist_destroy(ready, NULL);
    dlist_destroy(waiting, NULL);
    dlist_destroy(finished, NULL);

    free(manager_thread);
    free(main_thread);

    timer_delete(timer);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    exit(EXIT_SUCCESS);
}

dccthread_t *dccthread_create(const char *name, void (*func)(int), int param)
{
    sigprocmask(SIG_BLOCK, &mask, NULL);

    dccthread_t *thread = (dccthread_t *)malloc(sizeof(dccthread_t));
    getcontext(&(thread->context));
    strcpy(thread->name, name);

    thread->context.uc_link = &manager_thread->context;
    thread->context.uc_stack.ss_sp = thread->stack;
    thread->context.uc_stack.ss_size = THREAD_STACK_SIZE;
    thread->context.uc_stack.ss_flags = 0;
    thread->has_waited = 0;
    thread->waiting_for = NULL;
    sigemptyset(&thread->context.uc_sigmask);

    makecontext(&(thread->context), (void (*)(void))func, 1, param);

    dlist_push_right(ready, thread);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    return thread;
}

void dccthread_yield(void)
{
    sigprocmask(SIG_BLOCK, &mask, NULL);

    dccthread_t *current_thread = dccthread_self();
    dlist_push_right(ready, current_thread);
    swapcontext(&(current_thread->context), &(manager_thread->context));

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

void dccthread_exit(void)
{
    sigprocmask(SIG_BLOCK, &mask, NULL);

    dccthread_t *current_thread = dccthread_self();
    dlist_push_right(finished, current_thread);
    swapcontext(&(current_thread->context), &(manager_thread->context));
    setcontext(&manager_thread->context);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

void dccthread_wait(dccthread_t *tid)
{
    sigprocmask(SIG_BLOCK, &mask, NULL);

    dccthread_t *current_thread = dccthread_self();

    if (tid == NULL) {return;}

    tid->has_waited = 1;
    current_thread->waiting_for = tid;

    dlist_push_right(ready, current_thread);
    swapcontext(&current_thread->context, &manager_thread->context);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

// implementação da definição de cmp declarada e especificada em dlist.h
int dccthread_compare(const void *e1, const void *e2, void *userdata)
{
    return e1 != e2;
}

// função auxiliar para tratar o caso do fim de tempo de sono de uma thread (acordar a thread)
// remove a thread da lista de espera e adiciona à lista de prontas
void dccthread_wakeup(int signo, siginfo_t *si, void *context)
{
    dccthread_t *sleeping_thread = (dccthread_t *)si->si_value.sival_ptr;
    dlist_find_remove(waiting, sleeping_thread, dccthread_compare, NULL);
    dlist_push_right(ready, sleeping_thread);
}

// função auxiliar que inicializa o temporizador de sono, semelhante à timer_init
// porém, dessa vez, a ação não é tirar a thread da CPU (yield) e sim acordá-la (wakeup)
void timer_sleep_init(timer_t *timer_id, struct timespec *ts)
{
    // Configurar o manipulador de sinal para o temporizador
    action_sleep.sa_flags = SA_SIGINFO;
    action_sleep.sa_sigaction = dccthread_wakeup; //acordo a thread ao invés de retirá-la
    action_sleep.sa_mask = mask;
    sigaction(SIGRTMAX, &action_sleep, NULL);

    signalevent_sleep.sigev_notify = SIGEV_SIGNAL;
    signalevent_sleep.sigev_signo = SIGRTMAX;
    signalevent_sleep.sigev_value.sival_ptr = main_thread;

    timer_create(CLOCK_REALTIME, &signalevent_sleep, timer_id);

    timerspec_sleep.it_value.tv_sec = ts->tv_sec;
    timerspec_sleep.it_value.tv_nsec = ts->tv_nsec;
    timerspec_sleep.it_interval.tv_sec = SEC_VALUE;
    timerspec_sleep.it_interval.tv_nsec = NSEC_VALUE;

    timer_settime(*timer_id, 0, &timerspec_sleep, NULL);
}

void dccthread_sleep(struct timespec ts)
{
    sigprocmask(SIG_BLOCK, &mask, NULL);

    dccthread_timer timer;
    timer.sleep_time = ts;
    timer_sleep_init(&timer.timer_id, &ts);

    dlist_push_right(waiting, main_thread);
    swapcontext(&main_thread->context, &manager_thread->context);

    timer_delete(timer.timer_id);

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

dccthread_t *dccthread_self(void)
{
    return main_thread;
}

const char *dccthread_name(dccthread_t *tid)
{
    return tid->name;
}