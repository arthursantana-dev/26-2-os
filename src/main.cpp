// Barbearia do Sono: o problema do barbeiro sonolento como jogo de terminal.
// SSC0140 Sistemas Operacionais I, trabalho prático.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "config.hpp"
#include "dia.hpp"
#include "tela.hpp"

namespace {

struct Opcoes {
    bool demo = false;
    int duracaoDia = cfg::DURACAO_DIA_PADRAO;
};

void uso(const char* prog) {
    std::printf(
        "uso: %s [--demo] [--dia SEGUNDOS]\n"
        "  --demo          começa com o modo demonstração ligado (mutex dividido)\n"
        "  --dia SEGUNDOS  quanto tempo a porta fica aberta por dia (padrão %d)\n",
        prog, cfg::DURACAO_DIA_PADRAO);
}

std::optional<Opcoes> lerOpcoes(int argc, char** argv) {
    Opcoes o;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--demo")) {
            o.demo = true;
        } else if (!std::strcmp(argv[i], "--dia") && i + 1 < argc) {
            o.duracaoDia = std::atoi(argv[++i]);
            if (o.duracaoDia <= 0) return std::nullopt;
        } else {
            return std::nullopt;
        }
    }
    return o;
}

// Uma partida: dias seguidos até o jogador sair ou a reputação zerar.
void jogar(Tela& tela, const Opcoes& op, bool demo) {
    Progresso prog;
    while (true) {
        ResultadoDia r;
        {
            Dia dia(prog, demo, op.duracaoDia, tela);
            r = dia.executar();  // cria as threads do dia e faz join em todas
        }
        prog = r.prog;
        if (r.faliu) {
            tela.derrota(r);
            return;
        }
        if (r.abortou) return;

        // Loja entre os dias.
        while (true) {
            r.prog = prog;
            int c = tela.fimDoDia(r);
            if (c == '1' && prog.caixa >= cfg::CUSTO_CADEIRA && prog.cadeiras < cfg::MAX_CADEIRAS) {
                prog.caixa -= cfg::CUSTO_CADEIRA;
                ++prog.cadeiras;
            } else if (c == '2' && prog.caixa >= cfg::CUSTO_BARBEIRO &&
                       prog.contratados < cfg::MAX_CONTRATADOS) {
                prog.caixa -= cfg::CUSTO_BARBEIRO;
                ++prog.contratados;
            } else if (c == '\n') {
                break;
            } else if (c == 'Q') {
                return;
            }
        }
        ++prog.dia;
    }
}

}  // namespace

int main(int argc, char** argv) {
    auto op = lerOpcoes(argc, argv);
    if (!op) {
        uso(argv[0]);
        return 1;
    }

    Tela tela;
    bool demo = op->demo;
    while (true) {
        switch (tela.menu(demo)) {
            case '\n':
                jogar(tela, *op, demo);
                break;
            case 'M':
                tela.manual();
                break;
            case 'D':
                demo = !demo;
                break;
            case 'Q':
                return 0;
            default:
                break;
        }
    }
}
