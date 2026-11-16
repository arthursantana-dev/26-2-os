# Barbearia do Sono

Jogo de terminal para o trabalho prático de SSC0140 Sistemas Operacionais I.
É o problema do barbeiro sonolento (Aula 08) como jogo: você é o barbeiro.
Sem cliente, você dorme. Com as cadeiras de espera cheias, o próximo cliente
dá meia-volta e a barbearia perde reputação.

## Compilar e jogar

Requisitos: compilador com C++20 (GCC 11+ ou Clang 14+), `make` e ncurses.

```sh
# Ubuntu / Debian / WSL
sudo apt install build-essential libncurses-dev

# Fedora
sudo dnf install gcc-c++ make ncurses-devel

# macOS (ncurses já vem no sistema)
xcode-select --install
```

```sh
make          # gera ./barbearia
make run      # compila e abre o jogo
make demo     # abre com o modo demonstração ligado
make clean
```

Opções: `./barbearia --demo` liga o modo demonstração e `./barbearia --dia 20`
muda quantos segundos a porta fica aberta por dia (padrão 60). O terminal
precisa ter pelo menos 80x24.

### Controles

| Tecla | Onde | O que faz |
|---|---|---|
| Enter | menu | jogar |
| M | menu | manual |
| D | menu | liga/desliga o modo demonstração |
| A, S, D | jogo | cortar: apertar na ordem mostrada (rápido = gorjeta) |
| C | jogo | contratar um barbeiro (R$ 150), cria uma thread na hora |
| Q | jogo / menu | encerrar o dia / sair |
| 1, 2 | fim do dia | comprar cadeira de espera / contratar barbeiro |

## Decisões de implementação

Seguimos as sugestões do documento de opções do grupo:

| Decisão | Escolha |
|---|---|
| Linguagem e threads | C++20 com `std::thread` |
| Semáforos | `std::counting_semaphore` numa classe `Semaforo` com `down()` e `up()` e um contador ao lado para a tela |
| Interface | ncurses |
| Organização | uma thread por personagem, uma de entrada e uma só de desenho |
| Plataforma | Linux como alvo oficial, macOS funcionando, Windows pelo WSL |
| Compilação | Makefile |
| Modo demonstração | sim, desliga a proteção do mutex nas cadeiras |

Usamos `std::thread` com `join()` explícito em vez de `std::jthread` porque o
libc++ do macOS ainda trata `jthread` como experimental.

## Threads

| Thread | Quantas | O que faz |
|---|---|---|
| principal | 1 | menu, relógio do dia, encerramento e `join` em todas |
| porta | 1 | cria as threads dos clientes, cada vez mais rápido |
| cliente | uma por cliente | procura cadeira, espera a vez, é atendido e sai; sem cadeira, vai embora |
| barbeiro (você) | 1 | dorme sem cliente; o corte avança com as suas teclas |
| barbeiro contratado | uma por contratação | o mesmo ciclo, cortando sozinho |
| entrada | 1 | lê o teclado direto do terminal (`poll` + `read`) |
| desenho | 1 | copia o estado dentro do mutex e desenha fora dele; é a única que chama o ncurses durante o dia |

## Semáforos

Os três da solução dos slides:

| Semáforo | Início | Papel |
|---|---|---|
| `clientes` | 0 | clientes esperando. O barbeiro faz `down` e dorme se for zero; cada cliente que senta faz `up` |
| `barbeiros` | 0 | barbeiros livres. O cliente faz `down` e espera a vez; o barbeiro faz `up` quando chama o próximo |
| `mutex` | 1 | exclusão mútua das cadeiras de espera (e do resto do estado compartilhado) |

Para combinar o fim do corte entre a thread do barbeiro e a do cliente (o ponto
delicado apontado no documento), cada barbeiro tem mais dois:

| Semáforo | Início | Papel |
|---|---|---|
| `sentou[i]` | 0 | o cliente avisa ao barbeiro `i` que sentou na cadeira dele |
| `corte_feito[i]` | 0 | o barbeiro `i` avisa ao cliente que o corte acabou |

E um para a thread de entrada passar as teclas ao seu barbeiro (produtor e
consumidor): `teclas`, começa em 0.

Ciclo de cada um (`src/dia.cpp`):

```
barbeiro:                          cliente:
  loop                               down(mutex)
    down(clientes)   // dorme          se não há cadeira: up(mutex); vai embora
    up(barbeiros)    // chama          senta; up(mutex)
    down(sentou[i])                  up(clientes)        // acorda o barbeiro
    corta                            down(barbeiros)     // espera a vez
    up(corte_feito[i])               down(mutex); levanta; escolhe o barbeiro i que chamou; up(mutex)
                                     up(sentou[i])
                                     down(corte_feito[i])
```

Toda espera usa `try_acquire_for` em passos de 50 ms para desistir quando o dia
acaba ou o jogador sai, então o jogo nunca fica preso num `down`.

## Fim do dia

A principal fecha a porta quando o tempo acaba. Os clientes que já estão lá
dentro continuam sendo atendidos. Quando não sobra nenhum, a principal avisa
todas as threads, faz `up(clientes)` uma vez por barbeiro para acordar quem
está dormindo e faz `join` em todas antes de mostrar o placar.

## Modo demonstração

No modo normal, "contar a cadeira livre" e "sentar" ficam na mesma região
crítica. No modo demonstração, as duas partes ficam em regiões separadas, com
um atraso no meio. Quando dois clientes chegam juntos (acontece em ~35% das
chegadas), os dois contam a mesma cadeira livre e sentam um em cima do outro:
a cadeira fica vermelha com `2 !!` e o contador "sobreposições" sobe. O efeito
aparece quando o barbeiro está ocupado e a fila está formada.

Cada acesso à memória continua protegido; o erro mostrado é o de lógica
(check-then-act), sem comportamento indefinido no programa.

## Estrutura

```
Makefile
src/
  config.hpp     números do jogo (preços, tempos, limites)
  semaforo.hpp   Semaforo (down/up) e SecaoCritica (RAII do mutex)
  estado.hpp     estado compartilhado do dia e o quadro que vai para a tela
  dia.hpp/.cpp   as threads do jogo e a sincronização
  tela.hpp/.cpp  toda a interface em ncurses
  main.cpp       menu, sequência de dias e loja
```

## Procurar condição de corrida

```sh
make tsan
setarch $(uname -m) -R ./barbearia-tsan --dia 20 2> tsan.log
```

O `setarch -R` só é necessário em kernels recentes, onde o ThreadSanitizer
falha com "unexpected memory mapping".
