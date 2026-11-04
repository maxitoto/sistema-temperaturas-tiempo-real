#pragma once
#include "conexion/conexionApi.hpp"
#include "sensores/sensoresApi.hpp"
#include "sistema/sistemaApi.hpp"

namespace Apis {

inline void registrarTodasLasApis() {
    Conexion::registrarConexionApi();
    Sensores::registrarSensoresApi();
    Sistema::registrarSistemaApi();
}

} // namespace Apis