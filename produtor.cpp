#include <bits/stdc++.h>


produtor ()
{
   while (true)
   {
      pause = random (0..3)
      sleep (pause)
      down (s_vaga)
      down (s_buffer)
      item = random (0..999)
      insere item no buffer
      up (s_buffer)
      up (s_item)
      print item
   }






//-------------------------------------------------
// Confeitaria Produtor/Consumidor - C++17
// Compilar: g++ -std=c++17 -O2 -pthread confeitaria.cpp -o confeitaria
//
// O jogador e o PRODUTOR (thread principal). Cada cliente e uma thread CONSUMIDORA.
// Semaforos: mutex = 1, empty = N, full[tipo] = 0 (um por tipo de bolo).
// down() tem tempo limite e entra no fim de uma fila FIFO; up() acorda o primeiro da fila.

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

// ---------------------------------------------------------------- utilidades
static int rnd(int a, int b) {
    thread_local mt19937 gen{random_device{}()};
    return uniform_int_distribution<int>(a, b)(gen);
}

static mutex g_print;
static void say(const string& s) {
    lock_guard<mutex> lk(g_print);
    cout << s << endl;
}

// ----------------------------------------------------------------- semaforo
// Semaforo contador com fila FIFO de espera e down() com timeout.
class Semaphore {
    mutex m_;
    condition_variable cv_;
    int count_;
    int initial_;
    uint64_t next_ticket_ = 0;
    deque<uint64_t> fila_;   // threads bloqueadas, em ordem de chegada
    bool aborted_ = false;

public:
    explicit Semaphore(int initial = 0) : count_(initial), initial_(initial) {}

    // Retorna true se conseguiu decrementar; false se estourou o tempo (ou abort).
    bool down(milliseconds timeout) {
        unique_lock<mutex> lk(m_);
        uint64_t me = next_ticket_++;
        fila_.push_back(me);                       // entra no fim da fila
        bool ok = cv_.wait_for(lk, timeout, [&] {
            return aborted_ || (count_ > 0 && fila_.front() == me);  // so o 1o da fila passa
        });
        if (ok && !aborted_) {
            fila_.pop_front();
            --count_;
            cv_.notify_all();                      // o proximo da fila pode ter vaga tambem
            return true;
        }
        fila_.erase(find(fila_.begin(), fila_.end(), me));  // desistiu / abortou
        cv_.notify_all();
        return false;
    }

    void up() {
        lock_guard<mutex> lk(m_);
        ++count_;
        cv_.notify_all();                          // acorda; so o primeiro da fila segue
    }

    int value() {
        lock_guard<mutex> lk(m_);
        return count_;
    }

    void abort() {                                 // usado no fim de jogo/fase
        lock_guard<mutex> lk(m_);
        aborted_ = true;
        cv_.notify_all();
    }

    void reset() {                                 // so chamar sem threads esperando
        lock_guard<mutex> lk(m_);
        aborted_ = false;
        count_ = initial_;
        fila_.clear();
    }
};

// ------------------------------------------------------------------- jogo
constexpr int NT = 4;            // tipos de bolo por fase
constexpr int N = 6;             // capacidade do estoque
constexpr int VIDAS_INICIAIS = 3;
constexpr milliseconds FOREVER = hours(1);

const vector<string> NOMES_BOLOS = {"Chocolate", "Cenoura", "Morango", "Limao",
                                    "Coco", "Baunilha", "Laranja", "Fuba"};
const vector<string> INGREDIENTES = {"ovo", "farinha", "leite", "acucar", "manteiga",
                                     "fermento", "chocolate", "cenoura", "morango",
                                     "limao", "coco", "baunilha"};

struct Bolo {
    string nome;
    vector<string> receita;  // ingredientes NA ORDEM correta
};

// Estado global do jogo
static Semaphore mtx(1);                       // exclusao mutua do estoque
static Semaphore empty_(N);                    // vagas livres
static array<Semaphore, NT> full_;             // itens disponiveis por tipo
static array<deque<int>, NT> estoque;          // ids dos bolos (ordem de producao)
static array<atomic<int>, NT> qtd;             // so para exibicao
static array<Bolo, NT> menu;

static atomic<int> vidas{VIDAS_INICIAIS};
static atomic<int> pontos{0};
static atomic<int> resolvidos{0};              // clientes atendidos + perdidos na fase
static atomic<bool> parar{false};              // fim de jogo
static atomic<int> id_bolo{0};

static void abortar_tudo() {
    parar = true;
    mtx.abort();
    empty_.abort();
    for (auto& f : full_) f.abort();
}

static void gerar_menu(int fase) {
    vector<string> nomes = NOMES_BOLOS;
    shuffle(nomes.begin(), nomes.end(), mt19937{random_device{}()});
    int tam = min(5, 3 + fase / 3);            // receitas ficam maiores com as fases
    for (int t = 0; t < NT; ++t) {
        vector<string> ing = INGREDIENTES;
        shuffle(ing.begin(), ing.end(), mt19937{random_device{}()});
        ing.resize(tam);
        menu[t] = {nomes[t], ing};
    }
}

static void mostrar_menu() {
    ostringstream o;
    o << "\n========== MENU ==========\n";
    for (int t = 0; t < NT; ++t) {
        o << " " << (t + 1) << ") Bolo de " << menu[t].nome << ": ";
        for (size_t i = 0; i < menu[t].receita.size(); ++i)
            o << (i ? " -> " : "") << menu[t].receita[i];
        o << "\n";
    }
    o << "==========================\n"
      << "Produzir: <numero> <ingredientes na ordem>   ex: 1 ovo farinha leite\n"
      << "Comandos: m = menu | e = estoque | q = sair";
    say(o.str());
}

static string status_estoque() {
    ostringstream o;
    int total = 0;
    o << "Estoque: ";
    for (int t = 0; t < NT; ++t) {
        o << menu[t].nome << " x" << qtd[t].load() << "  ";
        total += qtd[t].load();
    }
    o << "(" << total << "/" << N << ")  Vidas: " << vidas << "  Pontos: " << pontos;
    return o.str();
}

// ------------------------------------------------------------- consumidor
// Cada cliente e uma thread. Se o bolo nao aparecer a tempo, ele vai embora.
static void consumidor(int cid, int tipo, int paciencia_s) {
    auto inicio = steady_clock::now();
    auto limite = inicio + seconds(paciencia_s);
    say("  [Cliente #" + to_string(cid) + "] chegou e quer Bolo de " + menu[tipo].nome +
        " (paciencia: " + to_string(paciencia_s) + "s)");

    // down(full[tipo]) com timeout = paciencia restante. Fila FIFO => ordem de chegada.
    auto restante = duration_cast<milliseconds>(limite - steady_clock::now());
    if (!full_[tipo].down(restante)) {
        if (!parar) {
            int v = --vidas;
            say("  [Cliente #" + to_string(cid) + "] cansou de esperar e foi embora! Vidas: " +
                to_string(max(v, 0)));
            if (v <= 0) abortar_tudo();
        }
        ++resolvidos;
        return;
    }

    // Regiao critica: retira o bolo do estoque
    int bolo = -1;
    if (mtx.down(FOREVER)) {
        bolo = estoque[tipo].front();
        estoque[tipo].pop_front();
        --qtd[tipo];
        mtx.up();
    }
    empty_.up();                               // liberou uma vaga para o produtor

    if (bolo < 0) { ++resolvidos; return; }

    // Pontuacao: base + bonus por rapidez
    double usado = duration<double>(steady_clock::now() - inicio).count();
    int bonus = max(0, static_cast<int>(100.0 * (1.0 - usado / paciencia_s)));
    int ganho = 100 + bonus;
    pontos += ganho;
    say("  [Cliente #" + to_string(cid) + "] comprou o bolo #" + to_string(bolo) +
        " de " + menu[tipo].nome + "! +" + to_string(ganho) + " pontos");
    ++resolvidos;
}

// Gera clientes ao longo do tempo (cada um em sua propria thread)
static void spawner(int total, double intervalo_s, int paciencia_s, int& prox_id) {
    vector<thread> clientes;
    for (int i = 0; i < total && !parar; ++i) {
        int ms = static_cast<int>(intervalo_s * 1000 * (0.5 + rnd(0, 100) / 100.0));
        this_thread::sleep_for(milliseconds(ms));
        if (parar) {
            // Clientes que nem chegaram contam como resolvidos para a fase poder terminar
            resolvidos += total - i;
            break;
        }
        clientes.emplace_back(consumidor, prox_id++, rnd(0, NT - 1), paciencia_s);
    }
    for (auto& c : clientes) c.join();
}

// ------------------------------------------------------------ entrada (stdin)
static mutex mq;
static condition_variable cvq;
static deque<string> linhas;
static bool eof_stdin = false;

static void leitor() {
    string l;
    while (getline(cin, l)) {
        lock_guard<mutex> lk(mq);
        linhas.push_back(l);
        cvq.notify_one();
    }
    lock_guard<mutex> lk(mq);
    eof_stdin = true;
    cvq.notify_one();
}

static bool pegar_linha(string& out, milliseconds t) {
    unique_lock<mutex> lk(mq);
    cvq.wait_for(lk, t, [] { return !linhas.empty() || eof_stdin; });
    if (linhas.empty()) return false;
    out = linhas.front();
    linhas.pop_front();
    return true;
}

// ---------------------------------------------------------------- produtor
static void produzir(const string& linha) {
    istringstream is(linha);
    int tipo;
    if (!(is >> tipo) || tipo < 1 || tipo > NT) {
        say("Comando invalido. Use: <numero do bolo> <ingredientes...>");
        return;
    }
    --tipo;
    vector<string> dado;
    string w;
    while (is >> w) {
        transform(w.begin(), w.end(), w.begin(), ::tolower);
        dado.push_back(w);
    }
    if (dado != menu[tipo].receita) {
        say("Receita errada! O bolo queimou e foi pro lixo.");
        return;
    }

    // down(empty): se o estoque esta cheio o produtor (jogador) TRAVA, com tempo limite
    if (empty_.value() == 0)
        say("Estoque cheio! Produtor travado esperando uma vaga...");
    if (!empty_.down(seconds(8))) {
        if (!parar) say("Estoque continuou cheio: o bolo foi descartado.");
        return;
    }
    if (mtx.down(FOREVER)) {
        int id = ++id_bolo;
        estoque[tipo].push_back(id);
        ++qtd[tipo];
        mtx.up();
        full_[tipo].up();                      // acorda o 1o cliente da fila desse tipo
        say("Bolo #" + to_string(id) + " de " + menu[tipo].nome + " colocado no estoque.");
    } else {
        empty_.up();
    }
}

// -------------------------------------------------------------------- main
int main() {
    ios::sync_with_stdio(false);
    cout << "=== CONFEITARIA PRODUTOR/CONSUMIDOR ===\n"
         << "Quantas fases? (0 = infinitas): " << flush;
    int fases_total = 0;
    if (!(cin >> fases_total) || fases_total < 0) fases_total = 0;
    string resto;
    getline(cin, resto);

    thread(leitor).detach();

    int prox_cliente = 1;
    bool venceu = true;

    for (int fase = 1; fases_total == 0 || fase <= fases_total; ++fase) {
        // reinicia semaforos e estoque para a fase
        mtx.reset();
        empty_.reset();
        for (int t = 0; t < NT; ++t) {
            full_[t].reset();
            estoque[t].clear();
            qtd[t] = 0;
        }
        resolvidos = 0;
        gerar_menu(fase - 1);

        int clientes = 4 + 2 * (fase - 1);
        double intervalo = max(1.5, 5.0 - 0.4 * (fase - 1));
        int paciencia = max(15, 35 - 2 * (fase - 1));

        say("\n######## FASE " + to_string(fase) + " ########  clientes: " + to_string(clientes) +
            " | paciencia: " + to_string(paciencia) + "s");
        mostrar_menu();

        int id_local = prox_cliente;
        thread th_spawn(spawner, clientes, intervalo, paciencia, ref(id_local));

        string linha;
        while (!parar && resolvidos < clientes) {
            if (!pegar_linha(linha, milliseconds(200))) {
                if (eof_stdin) { abortar_tudo(); venceu = false; }
                continue;
            }
            if (linha == "q") { abortar_tudo(); venceu = false; break; }
            else if (linha == "m") mostrar_menu();
            else if (linha == "e") say(status_estoque());
            else if (!linha.empty()) produzir(linha);
        }

        th_spawn.join();
        prox_cliente = id_local;

        if (parar) { if (vidas <= 0) venceu = false; break; }
        say("\n*** Fase " + to_string(fase) + " concluida! *** " + status_estoque());
    }

    say("\n=========== FIM DE JOGO ===========");
    say(string(venceu ? "Parabens, voce completou todas as fases!" : "Game over!") +
        "  Pontuacao final: " + to_string(pontos.load()));
    return 0;
}
