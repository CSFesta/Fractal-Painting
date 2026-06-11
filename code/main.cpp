#include <iostream>
#include <vector>
#include <queue>
#include <complex>
#include <utility>
#include <pthread.h>
#include <windows.h>
#include <winerror.h> // garante NO_ERROR para o gdiplus quando o pthread.h vem antes
#include <gdiplus.h>
using namespace std;
using namespace Gdiplus;

struct Cor {
    int r = 0, g = 0, b = 0; // componentes de 0 a 255 (comeca preto)
};

struct MandelbrotSet {
    // ---- dimensoes da imagem, em pixels ----
    int largura; // numero de colunas (eixo X)
    int altura;  // numero de linhas  (eixo Y)

    // ---- complexidade ----
    int max_iteracoes; // teto de iteracoes: + iteracoes = + detalhe e + lento

    // ---- janela do plano complexo que vamos desenhar ----
    double real_min, real_max; // faixa horizontal (eixo real)
    double imag_min, imag_max; // faixa vertical (eixo imaginario)

    // ---- a imagem: largura*altura cores, guardadas linha a linha ----
    vector<Cor> imagem;

    MandelbrotSet(int largura, int altura, int max_iteracoes,
                  double real_min, double real_max, double imag_centro)
        : largura(largura),
          altura(altura),
          max_iteracoes(max_iteracoes),
          real_min(real_min),
          real_max(real_max)
    {
        double altura_da_janela_imag = (real_max - real_min) * (double)altura / largura;
        imag_min = imag_centro - altura_da_janela_imag / 2.0;
        imag_max = imag_centro + altura_da_janela_imag / 2.0;
        imagem.resize((size_t)largura * altura); // todos os pixels comecam pretos
    }

    // Acesso conveniente a um pixel (x = coluna, y = linha) na imagem achatada.
    // Devolve Cor& (referencia) -> da para LER e ESCREVER: ex. pixel(x,y) = cor;
    Cor& pixel(int x, int y) {
        return imagem[(size_t)y * largura + x];
    }

    // Conta quantas iteracoes o ponto do pixel (px,py) aguenta antes de "escapar".
    // OBS: o std::complex so fica rapido compilando com -ffast-math (ver README).
    int calcular_iteracoes(int px, int py) const {
        // 1) converte a posicao do pixel no ponto complexo "c" (regra de tres)
        double fracao_horizontal = (double)px / largura;
        double fracao_vertical   = (double)py / altura;
        double c_real = real_min + fracao_horizontal * (real_max - real_min);
        double c_imag = imag_min + fracao_vertical   * (imag_max - imag_min);
        complex<double> c(c_real, c_imag);

        // 2) itera z = z*z + c, comecando em z = 0, ate |z| > 2 ou bater no teto.
        //    norm(z) ja e |z|^2 (real^2 + imag^2), sem raiz quadrada.
        complex<double> z(0.0, 0.0);
        int iteracoes = 0;
        while (iteracoes < max_iteracoes && norm(z) <= 4.0) {
            z = z * z + c; // faz (a^2-b^2)+(2ab)i e soma c automaticamente
            iteracoes++;
        }
        return iteracoes;
    }

    // Mapeia a contagem de iteracoes numa cor.
    Cor iteracoes_para_cor(int iteracoes) const {
        // Interior do conjunto
        if (iteracoes == max_iteracoes)
            return Cor{0, 0, 0};
    
        double t = (double)iteracoes / max_iteracoes;
    
        int r = (int)(9.0  * (1.0 - t) * t * t * t * 255.0);
        int g = (int)(15.0 * (1.0 - t) * (1.0 - t) * t * t * 255.0);
        int b = (int)(8.5  * (1.0 - t) * (1.0 - t) * (1.0 - t) * t * 255.0);
    
        return Cor{r, g, b};
    }
};


struct Tarefa {
    int x_inicio, x_fim; // colunas [x_inicio, x_fim)
    int y_inicio, y_fim; // linhas  [y_inicio, y_fim)
};

struct Resultado {
    int x_inicio, y_inicio; // canto superior esquerdo do bloco na imagem
    int largura, altura;    // dimensoes do bloco
    vector<Cor> pixels;     // cores do bloco, em ordem (linha a linha)
};

struct RecursosCompartilhados {
    // Ponteiro (o '*'): guarda o ENDERECO do MandelbrotSet, nao uma copia dele.
    // Assim todas as threads usam o MESMO objeto (e escrevem na mesma imagem).
    MandelbrotSet* mandelbrot = nullptr;

    queue<Tarefa> buffer_tarefas;       // produzido pela main, consumido pelos workers
    queue<Resultado> buffer_resultados; // produzido pelos workers, consumido pela impressora
    int total_tarefas = 0;              // quantas tarefas existem ao todo

    pthread_mutex_t mutex_tarefas      = PTHREAD_MUTEX_INITIALIZER; // protege o buffer de tarefas
    pthread_mutex_t mutex_resultados   = PTHREAD_MUTEX_INITIALIZER; // protege o buffer de resultados
    pthread_cond_t  cond_tem_resultado = PTHREAD_COND_INITIALIZER;  // avisa a impressora quando chega resultado
};

// 'recursos' vem por referencia (o '&'): a funcao mexe no objeto REAL, nao numa copia.
void montar_buffer_tarefas(RecursosCompartilhados& recursos, int tamanho_tarefa) {
    // *recursos.mandelbrot = o objeto apontado; o '&' faz 'mandelbrot' ser um apelido (sem copiar)
    MandelbrotSet& mandelbrot = *recursos.mandelbrot;
    for (int y = 0; y < mandelbrot.altura; y += tamanho_tarefa) {
        for (int x = 0; x < mandelbrot.largura; x += tamanho_tarefa) {
            Tarefa tarefa;
            tarefa.x_inicio = x;
            tarefa.y_inicio = y;
            // min(...) garante que o ultimo bloco nao passe da borda da imagem
            tarefa.x_fim = min(x + tamanho_tarefa, mandelbrot.largura);
            tarefa.y_fim = min(y + tamanho_tarefa, mandelbrot.altura);
            recursos.buffer_tarefas.push(tarefa);
        }
    }
    recursos.total_tarefas = (int)recursos.buffer_tarefas.size();
}

void* worker_trabalhador(void* argumento) {
    // 'argumento' e um void* (endereco generico que o pthread exige).
    // (RecursosCompartilhados*)argumento -> trata o endereco como sendo desse tipo;
    // o '*' desreferencia (pega o objeto) e o '&' faz 'recursos' ser um apelido dele.
    RecursosCompartilhados& recursos = *(RecursosCompartilhados*)argumento;
    MandelbrotSet& mandelbrot = *recursos.mandelbrot; // apelido curto p/ o mandelbrot

    while (true) {
        // 1) pega uma tarefa do buffer de tarefas (secao critica)
        pthread_mutex_lock(&recursos.mutex_tarefas);
        if (recursos.buffer_tarefas.empty()) {
            pthread_mutex_unlock(&recursos.mutex_tarefas);
            break; // acabaram as tarefas -> a thread encerra
        }
        Tarefa tarefa = recursos.buffer_tarefas.front();
        recursos.buffer_tarefas.pop();
        pthread_mutex_unlock(&recursos.mutex_tarefas);

        // 2) computa a tarefa: calcula a cor de cada pixel do bloco
        Resultado resultado;
        resultado.x_inicio = tarefa.x_inicio;
        resultado.y_inicio = tarefa.y_inicio;
        resultado.largura  = tarefa.x_fim - tarefa.x_inicio;
        resultado.altura   = tarefa.y_fim - tarefa.y_inicio;
        resultado.pixels.reserve((size_t)resultado.largura * resultado.altura);
        for (int py = tarefa.y_inicio; py < tarefa.y_fim; py++) {
            for (int px = tarefa.x_inicio; px < tarefa.x_fim; px++) {
                int iteracoes = mandelbrot.calcular_iteracoes(px, py);
                resultado.pixels.push_back(mandelbrot.iteracoes_para_cor(iteracoes));
            }
        }

        // 3) grava no buffer de resultados e avisa a impressora (secao critica)
        pthread_mutex_lock(&recursos.mutex_resultados);
        recursos.buffer_resultados.push(move(resultado));
        pthread_cond_signal(&recursos.cond_tem_resultado);
        pthread_mutex_unlock(&recursos.mutex_resultados);
    }
    return nullptr;
}

// ================== Janela de tempo real (Win32 + GDI) ==================
// Mostra a imagem sendo pintada bloco a bloco, enquanto o render roda.
// REGRA do Win32: a janela pertence a thread que a criou, e essa MESMA thread
// precisa processar as mensagens dela. Por isso tudo aqui roda na impressora.

static MandelbrotSet* g_mandelbrot = nullptr; // imagem que a janela mostra

// Desenha um retangulo da imagem direto na janela (converte Cor -> B,G,R,X do GDI).
void desenhar_na_janela(HWND janela, int x, int y, int largura, int altura) {
    vector<BYTE> bgra((size_t)largura * altura * 4);
    size_t i = 0;
    for (int py = y; py < y + altura; py++) {
        for (int px = x; px < x + largura; px++) {
            Cor c = g_mandelbrot->pixel(px, py);
            bgra[i++] = (BYTE)c.b;
            bgra[i++] = (BYTE)c.g;
            bgra[i++] = (BYTE)c.r;
            bgra[i++] = 0;
        }
    }
    // 32 bits por pixel; altura NEGATIVA = linha 0 e a de CIMA
    BITMAPINFO info = {};
    info.bmiHeader = { sizeof(BITMAPINFOHEADER), largura, -altura, 1, 32, BI_RGB };
    HDC contexto = GetDC(janela);
    SetDIBitsToDevice(contexto, x, y, largura, altura, 0, 0, 0, altura,
                      bgra.data(), &info, DIB_RGB_COLORS);
    ReleaseDC(janela, contexto);
}

// Funcao que o Windows chama para cada mensagem da janela (pintar, fechar, ...).
LRESULT CALLBACK proc_janela(HWND janela, UINT mensagem, WPARAM wparam, LPARAM lparam) {
    if (mensagem == WM_PAINT) { // o Windows pede para repintar (ex: foi descoberta)
        PAINTSTRUCT pintura;
        BeginPaint(janela, &pintura);
        desenhar_na_janela(janela, 0, 0, g_mandelbrot->largura, g_mandelbrot->altura);
        EndPaint(janela, &pintura);
        return 0;
    }
    if (mensagem == WM_DESTROY) { // a janela foi fechada
        PostQuitMessage(0);       // poe um WM_QUIT na fila para avisar o nosso loop
        return 0;
    }
    return DefWindowProcW(janela, mensagem, wparam, lparam); // o resto e padrao
}

// Cria a janela com a area util do tamanho exato da imagem (nullptr se falhar).
HWND criar_janela(int largura, int altura) {
    WNDCLASSW classe = {};
    classe.lpfnWndProc   = proc_janela;
    classe.hInstance     = GetModuleHandleW(nullptr);
    classe.lpszClassName = L"JanelaMandelbrot";
    classe.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&classe);

    RECT retangulo = {0, 0, largura, altura}; // area util -> tamanho total c/ borda
    AdjustWindowRect(&retangulo, WS_OVERLAPPEDWINDOW, FALSE);
    HWND janela = CreateWindowW(L"JanelaMandelbrot", L"Mandelbrot - pintando...",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                retangulo.right - retangulo.left,
                                retangulo.bottom - retangulo.top,
                                nullptr, nullptr, classe.hInstance, nullptr);
    if (janela != nullptr) ShowWindow(janela, SW_SHOW);
    return janela;
}

// Processa as mensagens pendentes sem bloquear
bool processar_mensagens() {
    MSG mensagem;
    while (PeekMessageW(&mensagem, nullptr, 0, 0, PM_REMOVE)) {
        if (mensagem.message == WM_QUIT) return false;
        TranslateMessage(&mensagem);
        DispatchMessageW(&mensagem);
    }
    return true;
}

void* thread_impressora(void* argumento) {
    // mesma ideia do worker: do void* de volta para o objeto real (por referencia)
    RecursosCompartilhados& recursos = *(RecursosCompartilhados*)argumento;
    MandelbrotSet& mandelbrot = *recursos.mandelbrot;

    g_mandelbrot = &mandelbrot;
    HWND janela = criar_janela(mandelbrot.largura, mandelbrot.altura);
    bool aberta = (janela != nullptr); // se falhar, renderiza sem visualizacao

    for (int i = 0; i < recursos.total_tarefas; i++) {
        // 1) pega um resultado (dorme esperando se o buffer estiver vazio)
        pthread_mutex_lock(&recursos.mutex_resultados);
        while (recursos.buffer_resultados.empty()) {
            pthread_cond_wait(&recursos.cond_tem_resultado, &recursos.mutex_resultados);
        }
        Resultado resultado = move(recursos.buffer_resultados.front());
        recursos.buffer_resultados.pop();
        pthread_mutex_unlock(&recursos.mutex_resultados);

        // 2) copia as cores do bloco para a posicao certa da imagem
        int indice = 0;
        for (int py = 0; py < resultado.altura; py++) {
            for (int px = 0; px < resultado.largura; px++) {
                mandelbrot.pixel(resultado.x_inicio + px, resultado.y_inicio + py)
                    = resultado.pixels[indice++];
            }
        }

        // 3) desenha o bloco na janela e processa as mensagens pendentes dela.
        if (aberta) {
            desenhar_na_janela(janela, resultado.x_inicio, resultado.y_inicio,
                               resultado.largura, resultado.altura);
            aberta = processar_mensagens();
        }
    }

    // 4) render completo: deixa a janela aberta ate o usuario fechar
    if (aberta) {
        SetWindowTextW(janela, L"Mandelbrot - concluido! (feche a janela para gravar o PNG)");
        cout << "Render concluido! Feche a janela para gravar o PNG e encerrar.\n";
        MSG mensagem;
        while (GetMessageW(&mensagem, nullptr, 0, 0) > 0) {
            TranslateMessage(&mensagem);
            DispatchMessageW(&mensagem);
        }
    }
    return nullptr;
}

void renderizar_em_paralelo(RecursosCompartilhados& recursos, int num_workers, int tamanho_tarefa) {
    montar_buffer_tarefas(recursos, tamanho_tarefa);
    cout << "Pintando " << recursos.total_tarefas << " tarefas com "
         << num_workers << " workers + 1 thread de impressao...\n";

    // sobe a thread de impressao (consumidora do buffer de resultados)
    pthread_t impressora;
    pthread_create(&impressora, nullptr, thread_impressora, &recursos);

    // sobe as threads trabalhadoras (consumidoras do buffer de tarefas)
    vector<pthread_t> workers(num_workers);
    for (int i = 0; i < num_workers; i++) {
        pthread_create(&workers[i], nullptr, worker_trabalhador, &recursos);
    }

    // espera todos os workers terminarem de computar
    for (int i = 0; i < num_workers; i++) {
        pthread_join(workers[i], nullptr);
    }

    // espera a impressora terminar (ela so retorna depois que o usuario
    // fechar a janela de visualizacao)
    pthread_join(impressora, nullptr);
}

// Localiza o "encoder" de PNG instalado no Windows e devolve o id dele (CLSID).
int obter_encoder_png_clsid(CLSID* clsid) {
    UINT quantidade = 0;
    UINT tamanho = 0;
    GetImageEncodersSize(&quantidade, &tamanho);
    if (tamanho == 0) return -1;

    vector<BYTE> buffer(tamanho);
    ImageCodecInfo* codecs = reinterpret_cast<ImageCodecInfo*>(buffer.data());
    GetImageEncoders(quantidade, tamanho, codecs);

    for (UINT i = 0; i < quantidade; i++) {
        if (wcscmp(codecs[i].MimeType, L"image/png") == 0) {
            *clsid = codecs[i].Clsid;
            return 0;
        }
    }
    return -1;
}

// Escreve a imagem atual em PNG. ASSUME que o GDI+ ja foi inicializado (na main).
// Como nao desliga o GDI+ aqui, pode ser chamada varias vezes durante o render.
void escrever_png(MandelbrotSet& mandelbrot, const wchar_t* nome_arquivo) {
    Bitmap bitmap(mandelbrot.largura, mandelbrot.altura, PixelFormat24bppRGB);
    BitmapData dados;
    Rect area(0, 0, mandelbrot.largura, mandelbrot.altura);

    if (bitmap.LockBits(&area, ImageLockModeWrite, PixelFormat24bppRGB, &dados) != Ok) {
        cerr << "Erro ao preparar a imagem para geracao do PNG!\n";
        return;
    }
    for (int y = 0; y < mandelbrot.altura; y++) {
        BYTE* linha = reinterpret_cast<BYTE*>(dados.Scan0) + y * dados.Stride;
        for (int x = 0; x < mandelbrot.largura; x++) {
            Cor c = mandelbrot.pixel(x, y);
            linha[x * 3 + 0] = static_cast<BYTE>(c.b); // o GDI+ guarda em ordem BGR
            linha[x * 3 + 1] = static_cast<BYTE>(c.g);
            linha[x * 3 + 2] = static_cast<BYTE>(c.r);
        }
    }
    bitmap.UnlockBits(&dados);

    CLSID clsid_png;
    if (obter_encoder_png_clsid(&clsid_png) != 0) {
        cerr << "Erro ao localizar o encoder PNG!\n";
        return;
    }
    bitmap.Save(nome_arquivo, &clsid_png, NULL);
    // o Bitmap e destruido aqui; o GDI+ continua ativo (so e desligado na main)
}

int main() {
    // ---- Parametros do programa ----
    const int LARGURA        = 1000;  // largura da imagem, em pixels
    const int ALTURA         = 1000;  // altura  da imagem, em pixels
    const int MAX_ITERACOES  = 10000; // complexidade do Mandelbrot
    const int NUM_WORKERS    = 24;    // nro de threads trabalhadoras
    const int TAMANHO_TAREFA = 32;   // cada tarefa e um bloco de TAMANHO_TAREFA x TAMANHO_TAREFA pixels

    // Para dar mais zoom, aproxime REAL_MIN/REAL_MAX e ajuste o centro.
    const double REAL_MIN    = -0.74877;
    const double REAL_MAX    = -0.74872;
    const double IMAG_CENTRO = 0.06505;

    cout << "Iniciando o programa...\n";

    // 1) cria o conjunto (parametros + imagem alocada + matematica pronta)
    MandelbrotSet mandelbrot(LARGURA, ALTURA, MAX_ITERACOES, REAL_MIN, REAL_MAX, IMAG_CENTRO);
    cout << "Imagem de " << mandelbrot.largura << " x " << mandelbrot.altura << " alocada.\n";

    // 2) prepara os recursos compartilhados e aponta para o conjunto
    RecursosCompartilhados recursos;
    recursos.mandelbrot = &mandelbrot; // '&' = ENDERECO de: aponta para o objeto real (sem copiar)

    // 3) inicia o GDI+ (necessario para gerar o PNG)
    GdiplusStartupInput gdiplus_input;
    ULONG_PTR gdiplus_token;
    if (GdiplusStartup(&gdiplus_token, &gdiplus_input, NULL) != Ok) {
        cerr << "Erro ao inicializar GDI+!\n";
        return 1;
    }

    // 4) renderiza em paralelo (workers + impressora)
    renderizar_em_paralelo(recursos, NUM_WORKERS, TAMANHO_TAREFA);

    // 5) gravacao final do PNG e desliga o GDI+
    escrever_png(mandelbrot, L"resultado.png");
    cout << "Arquivo 'resultado.png' gerado com sucesso na pasta do projeto!\n";
    GdiplusShutdown(gdiplus_token);

    cout << "Programa finalizado com sucesso!\n";
    return 0;
}
