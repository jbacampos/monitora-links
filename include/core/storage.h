#ifndef STORAGE_H
#define STORAGE_H

#include <LittleFS.h>
#include <type_traits>

#include "config/config.h"
#include "core/types.h"

//=============================================================================
// Sistema de arquivos
//=============================================================================

bool initFS();

//=============================================================================
// Persistência
//=============================================================================

template<typename T>
bool loadStorage(const char* file,
                 T& data,
                 uint32_t expectedMagic,
                 uint16_t expectedVersion) {
    File f = LittleFS.open(file, "r");

    if (!f) {
        DBG("%s inexistente.\n", file);
        return false;
    }

    if (f.size() != sizeof(T)) {
        DBG("Tamanho de %s incompatível\n", file);
        DBG("Arquivo : %u bytes\n", (unsigned)f.size());
        DBG("Esperado: %u bytes\n", sizeof(T));

        f.close();
        return false;
    }

    size_t lidos = f.read(reinterpret_cast<uint8_t*>(&data), sizeof(T));

    f.close();

    if (lidos != sizeof(T)) {
        DBG("Bytes lidos não batem com o tamanho de %s\n", file);
        DBG("Lidos.......: %u\n", lidos);
        DBG("Esperado....: %u\n", sizeof(T));
        return false;
    }

    if (expectedMagic != 0 && data.header.magic != expectedMagic) {
        DBG("MAGIC_NUMBER inválido em %s.\n", file);
        DBG("Lido........: %u\n", data.header.magic);
        DBG("Esperado....: %u\n", expectedMagic);
        return false;
    }

    if (expectedVersion != 0 && data.header.version != expectedVersion) {
        DBG("VERSION incompatível em %s.\n", file);
        DBG("Lido........: %u\n", data.header.version);
        DBG("Esperado....: %u\n", expectedVersion);
        return false;
    }

    return true;
}

template<typename T>
bool saveStorage(const char* file, const T& data)
{

    static_assert(!std::is_pointer<T>::value,
              "saveStorage(): passe o objeto, não um ponteiro.");

    File f = LittleFS.open(file, "w");

    if (!f) {
        DBG("Não foi possível gravar %s\n", file);
        return false;
    }

    size_t gravados =
        f.write(reinterpret_cast<const uint8_t*>(&data), sizeof(T));

    f.close();

    if (gravados != sizeof(T)) {
        DBG("Bytes gravados não batem com o tamanho de %s\n", file);
        DBG("Gravados : %u\n", gravados);
        DBG("Esperado : %u\n", sizeof(T));
        return false;
    }

    return true;
}


//=============================================================================
// Administração
//=============================================================================

void createDefaultState();

void createDefaultConfig();
void createDefaultRuntime();
void createDefaultEvents();

void resetConfig();
void resetRuntime();
void resetEvents();



#endif