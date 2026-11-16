#pragma once

#include <chrono>

// Números do jogo, num lugar só para facilitar o balanceamento.
namespace cfg {

inline constexpr int CADEIRAS_INICIAIS = 3;
inline constexpr int MAX_CADEIRAS = 6;
inline constexpr int MAX_CONTRATADOS = 3;
inline constexpr int MAX_BARBEIROS = 1 + MAX_CONTRATADOS;  // você + contratados

inline constexpr int PRECO_CORTE = 15;
inline constexpr int GORJETA_MAX = 10;
inline constexpr int CUSTO_CADEIRA = 100;
inline constexpr int CUSTO_BARBEIRO = 150;

inline constexpr int REPUTACAO_MAX = 100;
inline constexpr int PERDA_REPUTACAO = 10;  // cliente que vai embora sem cadeira
inline constexpr int GANHO_REPUTACAO = 2;   // cliente atendido

inline constexpr int DURACAO_DIA_PADRAO = 60;            // segundos com a porta aberta
inline constexpr double SEGUNDOS_CORTE_CONTRATADO = 3.5;  // corte automático
inline constexpr double CHANCE_DUPLA = 0.35;              // dois clientes chegam juntos

// Intervalo usado em toda espera que precisa checar se o dia acabou.
inline constexpr auto PASSO = std::chrono::milliseconds(50);

// Modo demonstração: atraso entre "contar a cadeira livre" e "sentar",
// para a condição de corrida aparecer na tela.
inline constexpr auto ATRASO_DEMO = std::chrono::milliseconds(300);

}  // namespace cfg
