#pragma once
#include "SensoresPosicion.h"

namespace ComunicacionServidor {
    enum class FaseGarita : uint8_t { LIBRE, PRESENCIA, ABIERTA, PASO, CANCELADA };
    struct Garita {
        char id[32]{};
        FaseGarita fase = FaseGarita::LIBRE;
    };
    enum class Accion : uint8_t { NINGUNA, ABRIR, ACK };
    struct Respuesta {
        char id[32]{};
        Accion accion = Accion::NINGUNA;
    };
    struct RespuestasGarita {
        Respuesta zona[2]{};
        unsigned long tomada = 0;
    };
    void iniciar();
    void publicar(const SensoresPosicion& sensores);
    void publicarGaritas(const Garita& entrada, const Garita& salida);
    bool recibirGaritas(RespuestasGarita& respuesta);
}
