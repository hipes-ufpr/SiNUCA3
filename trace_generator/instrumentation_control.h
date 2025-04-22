#ifndef INSTRUMENTATION_CONTROL_H
#define INSTRUMENTATION_CONTROL_H

// Inicia a definição de um bloco que será instrumentado.
// Após essa chamada, instrumentos de análise serão adicionados ao código.
__attribute__((noinline, used))
void BeginInstrumentationBlock(){}

// Finaliza a definição do bloco instrumentado.
// Nenhum novo instrumento de análise será inserida após essa chamada.
__attribute__((noinline, used))
void EndInstrumentationBlock(){}

// Habilita a execução das instruções de análise para a thread que chama a função.
// Deve ser chamada dentro de um bloco instrumentado.
__attribute__((noinline, used))
void EnableThreadInstrumentation(){}

// Desabilita a execução das instruções de análise para a thread que chama a função.
// Deve ser chamada dentro de um bloco instrumentado.
__attribute__((noinline, used))
void DisableThreadInstrumentation(){}

#endif
