// Archivo: src/tests/SQLite/sgbd/MotorSGBD.hpp
#pragma once
#include "CatalogoSistema.hpp"
#include "ModuloDDL.hpp"
#include "ModuloDML.hpp"
#include "ModuloDQL.hpp"
#include "ModuloDCL.hpp"
#include "ConsolaSQL.hpp"

namespace SGBD {

    class MotorSGBD {
    private:
        MotorSGBD() = default;
    public:
        static MotorSGBD& getInstance() {
            static MotorSGBD instance;
            return instance;
        }
        MotorSGBD(const MotorSGBD&) = delete;
        MotorSGBD& operator=(const MotorSGBD&) = delete;

        CatalogoSistema& catalogo() { return CatalogoSistema::getInstance(); }
        ModuloDDL&       ddl()      { return ModuloDDL::getInstance(); }
        ModuloDML&       dml()      { return ModuloDML::getInstance(); }
        ModuloDQL&       dql()      { return ModuloDQL::getInstance(); }
        ModuloDCL&       dcl()      { return ModuloDCL::getInstance(); }
        ConsolaSQL&      consola()  { return ConsolaSQL::getInstance(); }
    };

}