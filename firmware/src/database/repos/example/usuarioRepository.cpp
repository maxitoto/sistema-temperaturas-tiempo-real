#include "usuarioRepository.hpp"

std::vector<SGBD::Fila> obtenerUsuarioPorNombre(const char* nombre) {
    return db.from("Usuarios").select("*").eq("usuario", nombre).execute();
}