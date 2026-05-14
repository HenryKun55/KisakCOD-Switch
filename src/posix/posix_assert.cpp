// Stub do MyAssertHandler usado pelas macros iassert/vassert e por chamadas
// diretas em varios arquivos de src/universal/. No upstream Windows isso vai
// pra um dialog box + clipboard + breakpoint; aqui imprimimos no stderr e
// abortamos (comportamento aceitavel pra builds POSIX/Switch durante o porte).
//
// Quando integrarmos o renderer e tivermos uma overlay de debug, MyAssertHandler
// pode ser refeito pra mostrar a mensagem no jogo em vez de matar o processo.

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

void MyAssertHandler(const char *filename, int line, int /*type*/, const char *fmt, ...)
{
    std::fprintf(stderr, "[assert] %s:%d ", filename ? filename : "(null)", line);

    if (fmt)
    {
        std::va_list ap;
        va_start(ap, fmt);
        std::vfprintf(stderr, fmt, ap);
        va_end(ap);
    }

    std::fputc('\n', stderr);
    std::abort();
}
