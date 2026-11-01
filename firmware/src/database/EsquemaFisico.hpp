// Archivo: src/tests/SQLite/EsquemaFisico.hpp
#pragma once
#include "orm/ORM.hpp"

enum class EstadoTarjeta : int32_t {
    ACTIVA    = 1,
    BLOQUEADA = 2,
    VENCIDA   = 3
};

inline void iniciarEsquema() {
    // 1. Tabla Padre: Usuario
    db.createTable("Usuario")
      .addAtribute("id").typeInt().pk()
      .addAtribute("nombre").typeSrt().notNull()
      .addAtribute("dni").typeSrt().unique().notNull().varChar(8)
          .check([](const String& d) { return d.length() >= 4; }, "dni.size() >= 4");

    // 2. Tabla Hija: Tarjeta (Con FK apuntando a Usuario.id y borrado en CASCADE)
    db.createTable("Tarjeta")
      .addAtribute("id").typeInt().pk()
      .addAtribute("numero").typeSrt().notNull().unique()
      .addAtribute("fecha_vencimiento").typeSrt().notNull()
      .addAtribute("cvv").typeSrt().notNull().varChar(4)
      .addAtribute("id_usuario_fk").typeInt().notNull().FK("Usuario", "id", CASCADE)
      .addAtribute("estado").typeEnum({
                                        EstadoTarjeta::ACTIVA,
                                        EstadoTarjeta::BLOQUEADA,
                                        EstadoTarjeta::VENCIDA
                                      }).valueDefault(EstadoTarjeta::ACTIVA);

    // 3. Índice de búsqueda rápida
    db.createIndex("TarjetaUsuarioIdx")
      .attachTable("Tarjeta")
      .onAtribute("id_usuario_fk")
      .orderBy(ASC);
}