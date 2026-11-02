#include "usuarioRepository.hpp"

auto obtenerUsuarioPorNombre(char* nombre){
    auto usuario = db.from("Usuario").select("*").eq("nombre", nombre);
    return usuario;
}
