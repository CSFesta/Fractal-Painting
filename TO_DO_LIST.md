# Trabalho do GB — Fractal de Mandelbrot (concluído)

Arquitetura produtor-consumidor com dois buffers, conforme o quadro do professor:

```
 MAIN  --cria tarefas-->  [BUFFER DE TAREFAS]
 WORKERS (N) --pegam, computam, gravam-->  [BUFFER DE RESULTADOS]
 THREAD IMPRESSORA  --le os resultados e desenha na imagem ("tela")
```

## Itens concluídos
- [x] **struct da tarefa** — `Tarefa` (bloco cru, x/y) e `Resultado` (bloco já calculado).
- [x] **Thread mestra (main)** — fatia a imagem, monta o buffer de tarefas e administra as threads.
- [x] **Função controladora** — `montar_buffer_tarefas()` fatia a imagem em blocos de `TAMANHO_TAREFA x TAMANHO_TAREFA`.
- [x] **Threads trabalhadoras** — pegam tarefa → computam → gravam no buffer de resultados → próxima.
- [x] **Thread de impressão** — consome o buffer de resultados e desenha na imagem.
- [x] **Pintar pixel** — cor derivada do número de iterações do Mandelbrot (`iteracoes_para_cor`).
- [x] **Sincronização** — mutex nos dois buffers + condition variable avisando a impressora.
- [x] **Parâmetros do programa** — `NUM_WORKERS`, `TAMANHO_TAREFA`, `MAX_ITER`, dimensões e janela do plano complexo.

## Possíveis extensões
- Fazer também em MPI (+0,5 ponto, segundo o quadro).
- Pintar o pixel por id da thread (modo de depuração, para visualizar o fatiamento).
