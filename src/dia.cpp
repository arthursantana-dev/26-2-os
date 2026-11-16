#include "dia.hpp"

#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <random>
#include <string>

#include "tela.hpp"

using namespace std::chrono_literals;
using std::chrono::duration;
using std::chrono::milliseconds;

namespace {

double uniforme(double a, double b) {
    thread_local std::mt19937 rng{std::random_device{}()};
    return std::uniform_real_distribution<double>(a, b)(rng);
}

double segundosDesde(Relogio::time_point t0) {
    return duration<double>(Relogio::now() - t0).count();
}

std::string nomeBarbeiro(int i) {
    return i == 0 ? "você" : "barbeiro " + std::to_string(i + 1);
}

}  // namespace

Dia::Dia(const Progresso& prog, bool demo, int duracaoSegundos, Tela& tela)
    : tela_(tela), demo_(demo), duracao_(duracaoSegundos) {
    for (int i = 0; i < cfg::MAX_BARBEIROS; ++i) {
        sentou_[i] = std::make_unique<Semaforo>("sentou[" + std::to_string(i) + "]", 0);
        corteFeito_[i] = std::make_unique<Semaforo>("corte_feito[" + std::to_string(i) + "]", 0);
    }
    est_.prog = prog;
    est_.demo = demo;
    est_.cadeiras.resize(prog.cadeiras);
    Barbeiro voce;
    voce.jogador = true;
    est_.barbeiros.push_back(voce);
    for (int i = 0; i < prog.contratados; ++i) est_.barbeiros.push_back(Barbeiro{});
    est_.segundosRestantes = duracaoSegundos;
    est_.registrar("principal: abriu a barbearia, dia " + std::to_string(prog.dia));
}

Dia::~Dia() {
    // executar() já faz join em tudo; isto é só uma rede de segurança.
    cancelar_ = pararEntrada_ = pararDesenho_ = true;
    for (auto& t : threadsBarbeiros_)
        if (t.joinable()) t.join();
    for (auto& c : threadsClientes_)
        if (c.t.joinable()) c.t.join();
}

// ---------------------------------------------------------------------------
// Thread principal: cria as threads, cuida do relógio e encerra o dia.
// ---------------------------------------------------------------------------
ResultadoDia Dia::executar() {
    {
        std::lock_guard trava(threadsMtx_);
        for (int i = 0; i < static_cast<int>(est_.barbeiros.size()); ++i) iniciarBarbeiro(i);
    }
    std::thread tPorta(&Dia::porta, this);
    std::thread tDesenho(&Dia::desenho, this);
    std::thread tEntrada(&Dia::entrada, this);

    const auto inicio = Relogio::now();
    bool faliu = false;
    while (!cancelar_) {
        std::this_thread::sleep_for(cfg::PASSO);
        SecaoCritica s(mutex_);
        est_.segundosRestantes = std::max(0.0, duracao_ - segundosDesde(inicio));
        if (est_.portaAberta && est_.segundosRestantes <= 0) {
            est_.portaAberta = false;
            registrar("principal: porta fechada, atendendo quem ficou");
        }
        if (est_.prog.reputacao <= 0) {
            faliu = true;
            break;
        }
        // Porta fechada e nenhum cliente lá dentro: o dia acabou.
        if (!est_.portaAberta && est_.clientesAtivos == 0) break;
    }

    // Encerramento em ordem: ninguém fica preso num down, todos levam join.
    cancelar_ = true;
    pararEntrada_ = true;
    tEntrada.join();  // depois disso ninguém mais contrata barbeiro

    int nBarbeiros;
    {
        SecaoCritica s(mutex_);
        nBarbeiros = static_cast<int>(est_.barbeiros.size());
        registrar("principal: acorda os barbeiros e faz join em todos");
    }
    for (int i = 0; i < nBarbeiros; ++i) clientes_.up();  // acorda quem dorme

    tPorta.join();  // a porta faz join nos clientes
    for (auto& t : threadsBarbeiros_) t.join();
    threadsBarbeiros_.clear();

    pararDesenho_ = true;
    tDesenho.join();

    SecaoCritica s(mutex_);
    ResultadoDia r;
    r.prog = est_.prog;
    r.atendidos = est_.atendidos;
    r.foramEmbora = est_.foramEmbora;
    r.sobreposicoes = est_.sobreposicoes;
    r.segundosDormindo = est_.segundosDormindo;
    r.abortou = abortou_;
    r.faliu = faliu;
    return r;
}

// ---------------------------------------------------------------------------
// Porta: cria uma thread por cliente.
// ---------------------------------------------------------------------------
void Dia::porta() {
    double base;
    {
        SecaoCritica s(mutex_);
        base = std::max(1.2, 4.5 - 0.5 * (est_.prog.dia - 1));
    }

    while (!cancelar_) {
        double fracao;
        {
            SecaoCritica s(mutex_);
            if (!est_.portaAberta) break;
            fracao = 1.0 - est_.segundosRestantes / duracao_;
        }
        // Os clientes chegam cada vez mais rápido ao longo do dia.
        double intervalo = base * (1.0 - 0.4 * fracao) * uniforme(0.7, 1.3);
        if (!dormir(milliseconds(static_cast<int>(intervalo * 1000)))) break;

        int quantos = uniforme(0, 1) < cfg::CHANCE_DUPLA ? 2 : 1;
        for (int k = 0; k < quantos; ++k) {
            int id;
            {
                SecaoCritica s(mutex_);
                if (!est_.portaAberta) break;
                id = est_.proximoCliente++;
                ++est_.clientesAtivos;
                registrar("porta: criou a thread do cliente " + std::to_string(id));
            }
            auto& tc = threadsClientes_.emplace_back();
            tc.t = std::thread([this, id, &tc] {
                cliente(id);
                tc.terminou = true;
            });
        }

        // Recolhe as threads de clientes que já terminaram.
        for (auto it = threadsClientes_.begin(); it != threadsClientes_.end();) {
            if (it->terminou) {
                it->t.join();
                it = threadsClientes_.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (auto& c : threadsClientes_) c.t.join();
    threadsClientes_.clear();
}

// ---------------------------------------------------------------------------
// Cliente: procura cadeira, espera a vez, é atendido e vai embora.
// ---------------------------------------------------------------------------
void Dia::cliente(int id) {
    struct AoSair {
        Dia* d;
        ~AoSair() {
            SecaoCritica s(d->mutex_);
            --d->est_.clientesAtivos;
        }
    } aoSair{this};

    const std::string eu = "cliente " + std::to_string(id);
    int cadeira;

    if (!demo_) {
        // Contar a cadeira livre e sentar nela é UMA região crítica.
        SecaoCritica s(mutex_);
        cadeira = cadeiraLivre();
        if (cadeira >= 0) sentar(cadeira, id);
    } else {
        // MODO DEMONSTRAÇÃO: a contagem e o "sentar" ficam em regiões
        // separadas. Dois clientes que chegam juntos contam a mesma cadeira
        // livre e sentam um em cima do outro. (Cada acesso continua protegido
        // para não corromper a memória; o erro é de lógica, check-then-act.)
        {
            SecaoCritica s(mutex_);
            cadeira = cadeiraLivre();
        }
        std::this_thread::sleep_for(cfg::ATRASO_DEMO);
        if (cadeira >= 0) {
            SecaoCritica s(mutex_);
            if (!est_.cadeiras[cadeira].clientes.empty()) {
                ++est_.sobreposicoes;
                registrar(eu + ": sentou em cima de outro cliente!");
            }
            sentar(cadeira, id);
        }
    }

    if (cadeira < 0) {
        SecaoCritica s(mutex_);
        ++est_.foramEmbora;
        est_.prog.reputacao = std::max(0, est_.prog.reputacao - cfg::PERDA_REPUTACAO);
        est_.saidas.push_back({id, true, Relogio::now()});
        registrar(eu + ": sem cadeira, foi embora");
        return;
    }

    {
        SecaoCritica s(mutex_);
        registrar(eu + ": sentou, up(clientes), down(barbeiros)");
    }
    clientes_.up();  // acorda um barbeiro, se houver algum dormindo

    if (!barbeiros_.down(cancelar_)) {  // espera um barbeiro chamar
        SecaoCritica s(mutex_);
        levantar(cadeira, id);
        return;
    }

    // Um barbeiro fez up(barbeiros): procura qual está chamando e senta lá.
    int b = -1;
    {
        SecaoCritica s(mutex_);
        levantar(cadeira, id);
        for (int i = 0; i < static_cast<int>(est_.barbeiros.size()); ++i) {
            auto& bb = est_.barbeiros[i];
            if (bb.estado == EstadoBarbeiro::Chamando && bb.cliente < 0) {
                bb.cliente = id;
                b = i;
                break;
            }
        }
    }
    if (b < 0) return;  // só acontece no encerramento

    sentou_[b]->up();                             // avisa: estou na cadeira
    if (!corteFeito_[b]->down(cancelar_)) return;  // espera o corte acabar

    SecaoCritica s(mutex_);
    est_.saidas.push_back({id, false, Relogio::now()});
    registrar(eu + ": cabelo cortado, foi embora");
}

// ---------------------------------------------------------------------------
// Barbeiro: dorme sem cliente, chama o próximo, corta, repete.
// ---------------------------------------------------------------------------
void Dia::barbeiro(int i) {
    bool jogador;
    {
        SecaoCritica s(mutex_);
        jogador = est_.barbeiros[i].jogador;
    }
    const std::string eu = nomeBarbeiro(i);

    while (true) {
        {
            SecaoCritica s(mutex_);
            auto& b = est_.barbeiros[i];
            b.estado = EstadoBarbeiro::Dormindo;
            b.cliente = -1;
            b.sequencia.clear();
            b.progresso = 0;
        }

        const auto t0 = Relogio::now();
        bool ok = clientes_.down(cancelar_);  // dorme se não há cliente
        if (jogador) {
            SecaoCritica s(mutex_);
            est_.segundosDormindo += segundosDesde(t0);
        }
        if (!ok || cancelar_) break;

        {
            SecaoCritica s(mutex_);
            est_.barbeiros[i].estado = EstadoBarbeiro::Chamando;
            registrar(eu + ": acordou, up(barbeiros)");
        }
        barbeiros_.up();  // chama o próximo da fila

        if (!sentou_[i]->down(cancelar_)) break;  // espera o cliente sentar
        {
            SecaoCritica s(mutex_);
            est_.barbeiros[i].estado = EstadoBarbeiro::Cortando;
        }

        auto gorjeta = jogador ? cortarComTeclas(i) : cortarSozinho(i);
        corteFeito_[i]->up();  // libera o cliente (também no encerramento)
        if (!gorjeta) break;

        SecaoCritica s(mutex_);
        int ganho = cfg::PRECO_CORTE + *gorjeta;
        est_.prog.caixa += ganho;
        est_.prog.reputacao =
            std::min(cfg::REPUTACAO_MAX, est_.prog.reputacao + cfg::GANHO_REPUTACAO);
        ++est_.atendidos;
        registrar(eu + ": corte pronto, +R$ " + std::to_string(ganho) +
                  (*gorjeta > 0 ? " (gorjeta " + std::to_string(*gorjeta) + ")" : ""));
    }
}

// O corte do jogador: apertar as teclas na ordem. Quanto mais rápido, maior a
// gorjeta. As teclas chegam da thread de entrada pela fila + semáforo teclas.
std::optional<int> Dia::cortarComTeclas(int i) {
    int dia;
    {
        SecaoCritica s(mutex_);
        dia = est_.prog.dia;
    }
    const int n = 3 + std::min(3, (dia - 1) / 2);
    std::string seq;
    for (int k = 0; k < n; ++k) seq += "ASD"[static_cast<int>(uniforme(0, 3)) % 3];

    {
        // Descarta teclas apertadas antes do corte começar.
        SecaoCritica s(mutex_);
        while (teclas_.try_down()) {}
        filaTeclas_.clear();
        auto& b = est_.barbeiros[i];
        b.sequencia = seq;
        b.acertos = 0;
        b.errou = false;
    }

    const auto t0 = Relogio::now();
    int acertos = 0;
    while (acertos < n) {
        if (!teclas_.down(cancelar_)) return std::nullopt;
        SecaoCritica s(mutex_);
        if (filaTeclas_.empty()) continue;
        char k = filaTeclas_.front();
        filaTeclas_.pop_front();
        auto& b = est_.barbeiros[i];
        if (k == seq[acertos]) {
            ++acertos;
            b.errou = false;
        } else {
            acertos = 0;  // errou: recomeça a sequência
            b.errou = true;
        }
        b.acertos = acertos;
        b.progresso = static_cast<double>(acertos) / n;
    }

    double atraso = segundosDesde(t0) - 0.5 * n;  // ~0,5 s por tecla é "rápido"
    int gorjeta = cfg::GORJETA_MAX - static_cast<int>(std::lround(3.0 * std::max(0.0, atraso)));
    return std::clamp(gorjeta, 0, cfg::GORJETA_MAX);
}

// O corte de um barbeiro contratado: leva um tempo, sem gorjeta.
std::optional<int> Dia::cortarSozinho(int i) {
    const double total = cfg::SEGUNDOS_CORTE_CONTRATADO * uniforme(0.8, 1.2);
    const auto t0 = Relogio::now();
    while (true) {
        double feito = segundosDesde(t0) / total;
        {
            SecaoCritica s(mutex_);
            est_.barbeiros[i].progresso = std::min(1.0, feito);
        }
        if (feito >= 1.0) return 0;
        if (!dormir(100ms)) return std::nullopt;
    }
}

// ---------------------------------------------------------------------------
// Entrada: lê o teclado direto do terminal (sem chamar o ncurses, que não é
// seguro para várias threads) e repassa os comandos.
// ---------------------------------------------------------------------------
void Dia::entrada() {
    while (!pararEntrada_) {
        pollfd p{STDIN_FILENO, POLLIN, 0};
        if (poll(&p, 1, static_cast<int>(cfg::PASSO.count())) <= 0) continue;
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) continue;
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        switch (c) {
            case 'A':
            case 'S':
            case 'D': {
                {
                    SecaoCritica s(mutex_);
                    filaTeclas_.push_back(c);
                }
                teclas_.up();
                break;
            }
            case 'C':
                contratar();
                break;
            case 'Q':
                abortou_ = true;
                cancelar_ = true;
                break;
            default:
                break;
        }
    }
}

// Contratar cria uma thread de barbeiro no meio da partida.
void Dia::contratar() {
    std::lock_guard trava(threadsMtx_);
    int indice;
    {
        SecaoCritica s(mutex_);
        if (!est_.portaAberta) return;
        if (est_.prog.contratados >= cfg::MAX_CONTRATADOS) {
            registrar("entrada: já tem barbeiros demais");
            return;
        }
        if (est_.prog.caixa < cfg::CUSTO_BARBEIRO) {
            registrar("entrada: caixa insuficiente para contratar");
            return;
        }
        est_.prog.caixa -= cfg::CUSTO_BARBEIRO;
        ++est_.prog.contratados;
        indice = static_cast<int>(est_.barbeiros.size());
        est_.barbeiros.push_back(Barbeiro{});
        registrar("entrada: contratou, nova thread " + nomeBarbeiro(indice));
    }
    iniciarBarbeiro(indice);
}

void Dia::iniciarBarbeiro(int indice) {
    threadsBarbeiros_.emplace_back(&Dia::barbeiro, this, indice);
}

// ---------------------------------------------------------------------------
// Desenho: copia o estado dentro do mutex e desenha fora dele.
// ---------------------------------------------------------------------------
void Dia::desenho() {
    while (!pararDesenho_) {
        tela_.desenharDia(capturar());
        std::this_thread::sleep_for(cfg::PASSO);
    }
}

Quadro Dia::capturar() {
    Quadro q;
    {
        SecaoCritica s(mutex_);
        const auto agora = Relogio::now();
        std::erase_if(est_.saidas, [&](const Saida& x) { return agora - x.quando > 2500ms; });
        q.estado = est_;
    }
    const auto& e = q.estado;
    q.semaforos = {
        {"clientes", clientes_.valor(), e.prog.cadeiras, clientes_.bloqueados()},
        {"barbeiros", barbeiros_.valor(), static_cast<int>(e.barbeiros.size()),
         barbeiros_.bloqueados()},
        {"mutex", mutex_.valor(), 1, mutex_.bloqueados()},
    };
    // principal + porta + entrada + desenho + barbeiros + clientes
    q.threadsVivas = 4 + static_cast<int>(e.barbeiros.size()) + e.clientesAtivos;
    return q;
}

// ---------------------------------------------------------------------------
// Auxiliares (as de estado são chamadas com o mutex travado).
// ---------------------------------------------------------------------------
int Dia::cadeiraLivre() const {
    for (int i = 0; i < static_cast<int>(est_.cadeiras.size()); ++i)
        if (est_.cadeiras[i].clientes.empty()) return i;
    return -1;
}

void Dia::sentar(int cadeira, int id) { est_.cadeiras[cadeira].clientes.push_back(id); }

void Dia::levantar(int cadeira, int id) {
    std::erase(est_.cadeiras[cadeira].clientes, id);
}

void Dia::registrar(std::string linha) { est_.registrar(std::move(linha)); }

bool Dia::dormir(milliseconds duracao) {
    const auto fim = Relogio::now() + duracao;
    while (Relogio::now() < fim) {
        if (cancelar_) return false;
        auto falta = std::chrono::duration_cast<milliseconds>(fim - Relogio::now());
        std::this_thread::sleep_for(std::min<milliseconds>(cfg::PASSO, falta));
    }
    return !cancelar_;
}
