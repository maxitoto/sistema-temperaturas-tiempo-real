# Guía Práctica del SGBD y ORM (Del DER al Código)

## 1. Ciclo de Vida en `setup()`
Siempre respeta este orden al iniciar el ESP32:
1. `iniciarSGBDyORM()`: Monta la tarjeta MicroSD, crea la carpeta `/db` y activa la sesión administrativa.
2. `iniciarEsquema()`: Registra las tablas, tipos de datos, restricciones (`PK`, `FK`, `CHECK`) e índices.

---

## 2. Cómo llegar de una Tabla a otra (`JOIN`)
Cuando dos tablas están relacionadas (ej. `Tarjeta` tiene `id_usuario_fk` que apunta a `Usuario.id`), usa `.join()`:

### A. Join Automático (Recomendado)
Como el SGBD ya conoce la clave foránea por `iniciarEsquema()`, solo nombra la tabla:
```cpp
auto datos = db.from("Tarjeta")
               .select("fecha_vencimiento, cvv, Usuario.nombre")
               .join("Usuario")
               .eq("Usuario.dni", "42123456")
               .execute();