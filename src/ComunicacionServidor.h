#pragma once
#include "SensoresPosicion.h"

namespace ComunicacionServidor {
    void iniciar();
    // No ejecuta operaciones de red; publica una copia de los estados actuales.
    void publicar(const SensoresPosicion& sensores);
}
