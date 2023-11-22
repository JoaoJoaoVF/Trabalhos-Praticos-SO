<!-- LTeX: language=pt-BR -->

# PAGINADOR DE MEMÓRIA -- RELATÓRIO

1. Termo de compromisso

    Ao entregar este documento preenchiso, os membros do grupo afirmam que todo o código desenvolvido para este trabalho é de autoria própria.  Exceto pelo material listado no item 3 deste relatório, os membros do grupo afirmam não ter copiado material da Internet nem ter obtido código de terceiros.

2. Membros do grupo e alocação de esforço

    Preencha as linhas abaixo com o nome e o email dos integrantes do grupo.  Substitua marcadores `XX` pela contribuição de cada membro do grupo no desenvolvimento do trabalho (os valores devem somar 100%).

    * João Vitor Ferreira ferreirajoao@dcc.ufmg.br 34%
    * Lucas Roberto Santos Avelar lucasavelar@dcc.ufmg.br 33%
    * Luiza de Melo Gomes luizademelo@dcc.ufmg.br 33%

3. Referências bibliográficas

4. Detalhes de implementação

    1. Descreva e justifique as estruturas de dados utilizadas em sua solução.
        As principais estruturas de dados utilizadas na solução são:
        - page_t: Esta estrutura representa uma página de memória e contém informações como se a página é válida, o número do quadro, o número do bloco, se está alocada e o endereço da página.
        - page_table_t: Esta estrutura representa uma tabela de páginas para um processo específico e contém o ID do processo (PID), um ponteiro para as páginas, a quantidade de páginas e a capacidade máxima de páginas.
        - frames_t: Esta estrutura representa um quadro de memória física e contém o PID do processo que ocupa o quadro, o bit de referência usado pelo algoritmo da segunda chance e um ponteiro para a página associada ao quadro.
        - frame_list_t: Esta estrutura representa uma lista de quadros e contém o tamanho da lista, o tamanho da página, o índice usado pelo algoritmo da segunda chance e um ponteiro para os quadros.
        - blocks_t: Esta estrutura representa um bloco de armazenamento em disco e contém informações sobre se o bloco está alocado e um ponteiro para a página associada ao bloco.
        - block_list_t: Esta estrutura representa uma lista de blocos e contém o número total de blocos e um ponteiro para os blocos.
    2. Descreva o mecanismo utilizado para controle de acesso e modificação às páginas.
        O controle de acesso e modificação às páginas é feito através das funções auxiliares e do algoritmo da segunda chance implementado na função pager_fault.
        - Quando ocorre uma falta de página (pager_fault), a função check_page_validity é chamada para verificar se a página é válida ou não. Se a página for válida, a função handle_valid_page é chamada para atualizar as permissões de acesso à página e marcar o bit de referência como 1. Se a página for inválida, a função handle_invalid_page é chamada para alocar um novo quadro de memória e carregar a página do disco, se necessário.
        - O algoritmo da segunda chance é implementado na função find_frame_to_swap, que percorre a lista de quadros e verifica o bit de referência. Se o bit de referência for 0, o quadro é selecionado para ser trocado; caso contrário, o bit de referência é definido como 0 e o algoritmo continua procurando um quadro adequado.
        - A função swap é usada para trocar uma página quando não há quadros livres disponíveis. Ela marca a página removida como inválida e salva a página no disco, se necessário.
        - A função pager_syslog é usada para imprimir os bytes de uma página, tratando os acessos de leitura como se estivessem acessando a memória do processo.
        - Por fim, a função pager_destroy é chamada quando o processo termina, liberando todos os recursos alocados pelo processo, incluindo quadros de memória e blocos de disco.