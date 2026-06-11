## 1. Compilação
**g++ -O2 -ffast-math main.cpp -o main -lgdiplus -lgdi32 -pthread**

> O `-O2` liga as otimizações do compilador (faz uma diferença enorme em cálculo puro como esse).
> ⚠️ O `-ffast-math` é OBRIGATÓRIO: a `calcular_iteracoes` usa `std::complex`, que sem essa
> flag fica ~50x mais lento (a multiplicação de complexos chama uma função de biblioteca com
> checagem de Inf/NaN a cada passo). Com `-ffast-math` fica tão rápido quanto o cálculo na mão.
> O `-lgdiplus` é necessário para gerar o PNG; o `-lgdi32` para desenhar na janela de
> visualização; o `-pthread` para as threads.

## 2. Execução
**./main.exe**

Abre uma janela que mostra o fractal sendo pintado bloco a bloco, em tempo real, pela
`thread_impressora`. Quando o render termina, a janela fica aberta. Feche a janela para gravar o `resultado.png` e encerrar o programa.

### Parâmetros (editar no início da função `main()` em `main.cpp` e recompilar):
- **LARGURA / ALTURA** — dimensões da imagem, em pixels.
- **MAX_ITERACOES** — complexidade do Mandelbrot (mais = mais detalhe e mais lento).
- **NUM_WORKERS** — número de threads trabalhadoras. Referência boa: ~ número de núcleos da CPU.
- **TAMANHO_TAREFA** — cada tarefa é um bloco de `TAMANHO_TAREFA x TAMANHO_TAREFA` pixels.
- **REAL_MIN / REAL_MAX** — faixa horizontal (real) da janela. Aproxime os dois para dar zoom.
- **IMAG_CENTRO** — centro vertical da janela. A altura imaginária é derivada da proporção
  da imagem automaticamente (mantém os pixels quadrados, sem esticar o fractal).

### Saída:
Gera o arquivo **resultado.png** na pasta de execução.

## 3. Organização do código (`main.cpp`)
1. **Cor** — a cor de um pixel.
2. **MandelbrotSet** — parâmetros + imagem + a matemática do fractal (encapsula tudo).
3. **Paralelização** — `Tarefa`, `Resultado`, `RecursosCompartilhados` (os dois buffers + mutexes/cond),
   e as threads `worker_trabalhador` / `thread_impressora`.
4. **Saída** — geração do PNG via GDI+.
5. **main** — define os parâmetros e dispara a renderização.
