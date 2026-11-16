#include "tela.hpp"

#include <ncurses.h>

#include <algorithm>
#include <cctype>
#include <clocale>
#include <cstdio>
#include <string>
#include <vector>

namespace {

enum Cor : short { NENHUMA = 0, AMARELO, AZUL, VERDE, VERMELHO, ROXO, CINZA };

constexpr int W = 80;
constexpr int H = 24;
int OX = 0;
int OY = 0;

// Largura em colunas de um texto UTF-8 (todos os nossos caracteres ocupam 1).
int largura(const std::string& s) {
    int n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++n;
    return n;
}

attr_t atributo(Cor cor, attr_t extra) {
    attr_t a = extra;
    if (cor != NENHUMA && has_colors()) a |= COLOR_PAIR(cor);
    if (cor == CINZA) a |= A_DIM;
    return a;
}

void escrever(int y, int x, const std::string& s, Cor cor = NENHUMA, attr_t extra = A_NORMAL) {
    attr_t a = atributo(cor, extra);
    attron(a);
    mvaddstr(OY + y, OX + x, s.c_str());
    attroff(a);
}

void centro(int y, const std::string& s, Cor cor = NENHUMA, attr_t extra = A_NORMAL) {
    escrever(y, (W - largura(s)) / 2, s, cor, extra);
}

void direita(int y, int xFim, const std::string& s, Cor cor = NENHUMA, attr_t extra = A_NORMAL) {
    escrever(y, xFim - largura(s), s, cor, extra);
}

void caixa(int y, int x, int h, int w, Cor cor = CINZA) {
    attr_t a = atributo(cor, A_NORMAL);
    attron(a);
    y += OY;
    x += OX;
    mvaddch(y, x, ACS_ULCORNER);
    mvhline(y, x + 1, ACS_HLINE, w - 2);
    mvaddch(y, x + w - 1, ACS_URCORNER);
    mvvline(y + 1, x, ACS_VLINE, h - 2);
    mvvline(y + 1, x + w - 1, ACS_VLINE, h - 2);
    mvaddch(y + h - 1, x, ACS_LLCORNER);
    mvhline(y + h - 1, x + 1, ACS_HLINE, w - 2);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
    attroff(a);
}

// Centraliza a área de 80x24 no terminal. Devolve false se não couber.
bool prepararQuadro() {
    erase();
    if (COLS < W || LINES < H) {
        mvaddstr(0, 0, "Aumente o terminal para pelo menos 80x24.");
        refresh();
        return false;
    }
    OX = (COLS - W) / 2;
    OY = (LINES - H) / 2;
    return true;
}

std::string mmss(double segundos) {
    int s = std::max(0, static_cast<int>(segundos + 0.5));
    char buf[16];
    std::snprintf(buf, sizeof buf, "%d:%02d", s / 60, s % 60);
    return buf;
}

std::string id2(int id) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d", id);
    return buf;
}

}  // namespace

Tela::Tela() {
    std::setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    set_escdelay(25);
    // Durante o dia quem lê o teclado é a thread de entrada, direto do
    // terminal. Isto impede o ncurses de espiar a entrada no refresh().
    typeahead(-1);
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(AMARELO, COLOR_YELLOW, -1);
        init_pair(AZUL, COLOR_BLUE, -1);
        init_pair(VERDE, COLOR_GREEN, -1);
        init_pair(VERMELHO, COLOR_RED, -1);
        init_pair(ROXO, COLOR_MAGENTA, -1);
        init_pair(CINZA, COLOR_WHITE, -1);
    }
}

Tela::~Tela() { endwin(); }

int Tela::esperarTecla() {
    flushinp();
    while (true) {
        int c = getch();
        if (c == KEY_RESIZE) return c;
        if (c == KEY_ENTER || c == '\r' || c == '\n') return '\n';
        if (c >= 0 && c < 256) return std::toupper(c);
    }
}

// ---------------------------------------------------------------------------
int Tela::menu(bool demo) {
    while (true) {
        if (prepararQuadro()) {
            centro(5, "BARBEARIA DO SONO", AMARELO, A_BOLD);
            centro(7, "corte rápido, não deixe a fila ir embora");
            for (int k = 0; k < 3; ++k) caixa(9, 29 + k * 8, 3, 6);
            escrever(9, 54, "zZ", AZUL, A_BOLD);
            centro(14, "[Enter] jogar", VERDE, A_BOLD);
            centro(16, std::string("[M] manual · [D] modo demonstração: ") +
                           (demo ? "LIGADO" : "desligado") + " · [Q] sair",
                   CINZA);
            if (demo)
                centro(18, "modo demonstração: o mutex das cadeiras fica dividido em dois",
                       VERMELHO);
            centro(21, "Nenhuma thread de jogo existe ainda. Só a principal,", CINZA);
            centro(22, "que desenha o menu e espera uma tecla.", CINZA);
            refresh();
        }
        int c = esperarTecla();
        if (c != KEY_RESIZE) return c;
    }
}

// ---------------------------------------------------------------------------
void Tela::manual() {
    static const std::vector<std::pair<std::string, Cor>> linhas = {
        {"MANUAL", AMARELO},
        {"", NENHUMA},
        {"Você é o barbeiro. Sem cliente, você dorme. Com as cadeiras de espera", NENHUMA},
        {"cheias, o próximo cliente dá meia-volta e a barbearia perde reputação.", NENHUMA},
        {"", NENHUMA},
        {"CONTROLES", AMARELO},
        {"  A, S, D   cortar: aperte as teclas na ordem mostrada (rápido = gorjeta)", NENHUMA},
        {"  C         contratar um barbeiro (R$ 150): cria uma thread na hora", NENHUMA},
        {"  Q         encerrar o dia e voltar ao menu", NENHUMA},
        {"", NENHUMA},
        {"THREADS", AMARELO},
        {"  cliente (uma por cliente), barbeiro (você e os contratados),", NENHUMA},
        {"  porta (cria os clientes), entrada (teclado), desenho (tela)", NENHUMA},
        {"", NENHUMA},
        {"SEMÁFOROS", AMARELO},
        {"  clientes  = 0  clientes esperando; o barbeiro dorme no down", NENHUMA},
        {"  barbeiros = 0  barbeiros livres; o cliente espera a vez no down", NENHUMA},
        {"  mutex     = 1  protege o contador de cadeiras ocupadas", NENHUMA},
        {"", NENHUMA},
        {"Fim do dia: a porta fecha, os últimos são atendidos e a principal faz", NENHUMA},
        {"join em todas as threads. Depois, gaste o caixa em cadeiras ou barbeiros.", NENHUMA},
        {"", NENHUMA},
        {"[qualquer tecla] voltar", CINZA},
    };
    while (true) {
        if (prepararQuadro()) {
            for (int i = 0; i < static_cast<int>(linhas.size()); ++i)
                escrever(i, 2, linhas[i].first, linhas[i].second,
                         linhas[i].second == AMARELO ? A_BOLD : A_NORMAL);
            refresh();
        }
        if (esperarTecla() != KEY_RESIZE) return;
    }
}

// ---------------------------------------------------------------------------
// Chamado só pela thread de desenho.
void Tela::desenharDia(const Quadro& q) {
    if (!prepararQuadro()) return;
    const Estado& e = q.estado;

    // Cabeçalho
    escrever(0, 1, "BARBEARIA DO SONO", AMARELO, A_BOLD);
    direita(0, 79,
            "caixa R$ " + std::to_string(e.prog.caixa) + " · dia " + std::to_string(e.prog.dia) +
                " · reputação " + std::to_string(e.prog.reputacao));
    if (e.demo) escrever(1, 1, "MODO DEMONSTRAÇÃO: mutex dividido", VERMELHO, A_BOLD);
    if (e.portaAberta)
        direita(1, 79, "porta fecha em " + mmss(e.segundosRestantes), CINZA);
    else
        direita(1, 79, "porta fechada: atendendo quem ficou", VERMELHO);
    attron(atributo(CINZA, A_NORMAL));
    mvhline(OY + 2, OX, ACS_HLINE, W);
    attroff(atributo(CINZA, A_NORMAL));

    // Porta e quem está saindo
    escrever(3, 1, "porta", CINZA);
    caixa(4, 1, 7, 5, e.portaAberta ? CINZA : VERMELHO);
    int y = 4;
    for (auto it = e.saidas.rbegin(); it != e.saidas.rend() && y <= 9; ++it, ++y) {
        if (it->irritado)
            escrever(y, 8, "← " + id2(it->cliente) + " sem cadeira", VERMELHO, A_BOLD);
        else
            escrever(y, 8, "← " + id2(it->cliente) + " corte novo", VERDE);
    }

    // Cadeiras dos barbeiros
    escrever(3, 32, "CADEIRAS DE BARBEIRO", CINZA);
    for (int i = 0; i < static_cast<int>(e.barbeiros.size()); ++i) {
        const Barbeiro& b = e.barbeiros[i];
        const int x = 32 + i * 12;
        escrever(4, x, b.jogador ? "VOCÊ" : "BARBEIRO " + std::to_string(i + 1),
                 b.jogador ? AMARELO : ROXO, A_BOLD);
        caixa(5, x, 3, 10, b.jogador ? AMARELO : CINZA);
        switch (b.estado) {
            case EstadoBarbeiro::Dormindo:
                escrever(6, x + 3, "zZz", AZUL, A_BOLD);
                escrever(8, x, "dormindo", AZUL);
                break;
            case EstadoBarbeiro::Chamando:
                escrever(8, x, "chamando", AMARELO);
                break;
            case EstadoBarbeiro::Cortando:
                escrever(8, x, "cortando", VERDE);
                break;
        }
        if (b.cliente >= 0) escrever(6, x + 3, "(" + id2(b.cliente) + ")", VERDE, A_BOLD);

        if (b.estado == EstadoBarbeiro::Cortando) {
            if (b.jogador && !b.sequencia.empty()) {
                for (int k = 0; k < static_cast<int>(b.sequencia.size()); ++k) {
                    std::string t(1, b.sequencia[k]);
                    if (k < b.acertos)
                        escrever(9, x + 2 * k, t, VERDE, A_REVERSE | A_BOLD);
                    else if (k == b.acertos)
                        escrever(9, x + 2 * k, t, NENHUMA, A_BOLD | A_UNDERLINE);
                    else
                        escrever(9, x + 2 * k, t, CINZA);
                }
                if (b.errou) escrever(10, x, "errou!", VERMELHO, A_BOLD);
            } else if (!b.jogador) {
                int cheio = static_cast<int>(b.progresso * 10 + 0.5);
                std::string barra;
                for (int k = 0; k < 10; ++k) barra += k < cheio ? "■" : "□";
                escrever(9, x, barra, ROXO);
            }
        }
    }

    // Cadeiras de espera
    escrever(12, 1, "CADEIRAS DE ESPERA", CINZA);
    for (int k = 0; k < static_cast<int>(e.cadeiras.size()); ++k) {
        const auto& c = e.cadeiras[k].clientes;
        const int x = 1 + k * 7;
        Cor cor = c.size() > 1 ? VERMELHO : c.size() == 1 ? VERDE : CINZA;
        caixa(13, x, 3, 6, cor);
        if (c.size() == 1) escrever(14, x + 2, id2(c[0]), VERDE, A_BOLD);
        if (c.size() > 1) escrever(14, x + 1, std::to_string(c.size()) + " !!", VERMELHO, A_BOLD);
    }

    // Números do dia
    escrever(12, 50, "atendidos", CINZA);
    direita(12, 79, std::to_string(e.atendidos));
    escrever(13, 50, "foram embora", CINZA);
    direita(13, 79, std::to_string(e.foramEmbora), e.foramEmbora ? VERMELHO : NENHUMA);
    escrever(14, 50, "tempo dormindo", CINZA);
    direita(14, 79, mmss(e.segundosDormindo));
    escrever(15, 50, "threads vivas", CINZA);
    direita(15, 79, std::to_string(q.threadsVivas));
    if (e.demo) {
        escrever(16, 50, "sobreposições", VERMELHO);
        direita(16, 79, std::to_string(e.sobreposicoes), VERMELHO, A_BOLD);
    }

    // O que as threads estão fazendo
    escrever(17, 1, "O QUE AS THREADS ESTÃO FAZENDO", CINZA);
    int linha = 18;
    for (int i = 0; i < static_cast<int>(e.log.size()); ++i, ++linha) {
        bool ultima = i + 1 == static_cast<int>(e.log.size());
        escrever(linha, 2, e.log[i], ultima ? NENHUMA : CINZA, ultima ? A_BOLD : A_NORMAL);
    }

    // Controles e faixa dos semáforos
    escrever(22, 1, "[A][S][D] cortar na ordem · [C] contratar R$ 150 · [Q] encerrar", CINZA);
    int x = 1;
    for (const auto& s : q.semaforos) {
        bool preso = s.valor <= 0 && s.bloqueados > 0;
        escrever(23, x, s.nome, preso ? VERMELHO : CINZA, preso ? A_BOLD : A_NORMAL);
        x += largura(s.nome) + 1;
        for (int k = 0; k < s.maximo; ++k, ++x)
            escrever(23, x, k < s.valor ? "■" : "□", k < s.valor ? VERDE : CINZA);
        std::string v = " " + std::to_string(s.valor);
        escrever(23, x, v, NENHUMA, A_BOLD);
        x += largura(v);
        if (s.bloqueados > 0) {
            std::string b = " (" + std::to_string(s.bloqueados) + " no down)";
            escrever(23, x, b, AZUL);
            x += largura(b);
        }
        x += 3;
    }

    refresh();
}

// ---------------------------------------------------------------------------
int Tela::fimDoDia(const ResultadoDia& r) {
    while (true) {
        if (prepararQuadro()) {
            const Progresso& p = r.prog;
            caixa(1, 16, 21, 48, AMARELO);
            centro(3, "FIM DO DIA " + std::to_string(p.dia), AMARELO, A_BOLD);

            int y = 5;
            auto linha = [&](const std::string& rotulo, const std::string& valor, Cor cor) {
                escrever(y, 20, rotulo, CINZA);
                direita(y, 60, valor, cor, A_BOLD);
                ++y;
            };
            linha("Caixa", "R$ " + std::to_string(p.caixa), NENHUMA);
            linha("Clientes atendidos", std::to_string(r.atendidos), VERDE);
            linha("Foram embora", std::to_string(r.foramEmbora), r.foramEmbora ? VERMELHO : NENHUMA);
            linha("Tempo dormindo", mmss(r.segundosDormindo), AZUL);
            linha("Reputação", std::to_string(p.reputacao), NENHUMA);
            if (r.sobreposicoes > 0)
                linha("Clientes sentados em cima de outro", std::to_string(r.sobreposicoes), VERMELHO);

            y = 13;
            escrever(y++, 20, "LOJA", AMARELO, A_BOLD);
            auto item = [&](const std::string& tecla, const std::string& nome, int custo, int tem,
                            int max) {
                bool pode = p.caixa >= custo && tem < max;
                escrever(y, 20, tecla + " " + nome, pode ? NENHUMA : CINZA);
                direita(y, 60,
                        "R$ " + std::to_string(custo) + "  (" + std::to_string(tem) + "/" +
                            std::to_string(max) + ")",
                        pode ? AMARELO : CINZA);
                ++y;
            };
            item("[1]", "cadeira de espera nova", cfg::CUSTO_CADEIRA, p.cadeiras, cfg::MAX_CADEIRAS);
            item("[2]", "contratar barbeiro", cfg::CUSTO_BARBEIRO, p.contratados,
                 cfg::MAX_CONTRATADOS);

            centro(19, "[Enter] próximo dia · [Q] voltar ao menu", VERDE);
            refresh();
        }
        int c = esperarTecla();
        if (c != KEY_RESIZE) return c;
    }
}

void Tela::derrota(const ResultadoDia& r) {
    while (true) {
        if (prepararQuadro()) {
            caixa(4, 16, 14, 48, VERMELHO);
            centro(6, "BARBEARIA FECHADA", VERMELHO, A_BOLD);
            centro(8, "a reputação chegou a zero no dia " + std::to_string(r.prog.dia));
            centro(10, "caixa final: R$ " + std::to_string(r.prog.caixa));
            centro(11, "atendidos hoje: " + std::to_string(r.atendidos) +
                           " · foram embora: " + std::to_string(r.foramEmbora));
            centro(15, "[qualquer tecla] voltar ao menu", CINZA);
            refresh();
        }
        if (esperarTecla() != KEY_RESIZE) return;
    }
}
