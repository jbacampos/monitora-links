#ifndef STORAGE_H
#define STORAGE_H

#include <LittleFS.h>
#include <cstddef>
#include <type_traits>

#include "config/config.h"
#include "core/types.h"

//=============================================================================
// Compatibilidade binária dos arquivos persistidos
//=============================================================================
//
// loadStorage() só aceita um arquivo quando o tamanho é exatamente sizeof(T).
// Event e PendingNotification receberam o campo resetReasonRaw no byte de
// preenchimento final: nenhum offset existente e nenhum tamanho mudaram, de
// modo que events.bin e runtime.bin continuam compatíveis com o histórico já
// gravado. As verificações abaixo travam esse layout; uma mudança futura
// falha na compilação em vez de invalidar silenciosamente os dados.

static_assert(sizeof(Event) == 24, "Event: tamanho persistido alterado");
static_assert(offsetof(Event, inicio) == 0, "Event: offset de inicio alterado");
static_assert(offsetof(Event, fim) == 4, "Event: offset de fim alterado");
static_assert(offsetof(Event, id) == 8, "Event: offset de id alterado");
static_assert(offsetof(Event, duracaoSeg) == 12, "Event: offset de duracaoSeg alterado");
static_assert(offsetof(Event, rssi) == 16, "Event: offset de rssi alterado");
static_assert(offsetof(Event, link) == 18, "Event: offset de link alterado");
static_assert(offsetof(Event, motivo) == 19, "Event: offset de motivo alterado");
static_assert(offsetof(Event, bootReason) == 20, "Event: offset de bootReason alterado");
static_assert(offsetof(Event, tipo) == 21, "Event: offset de tipo alterado");
static_assert(offsetof(Event, enviadoTelegram) == 22, "Event: offset de enviadoTelegram alterado");
static_assert(offsetof(Event, resetReasonRaw) == 23, "Event: resetReasonRaw fora do padding final");

static_assert(sizeof(PendingNotification) == 24, "PendingNotification: tamanho persistido alterado");
static_assert(offsetof(PendingNotification, inicio) == 0, "PendingNotification: offset de inicio alterado");
static_assert(offsetof(PendingNotification, fim) == 4, "PendingNotification: offset de fim alterado");
static_assert(offsetof(PendingNotification, evento) == 8, "PendingNotification: offset de evento alterado");
static_assert(offsetof(PendingNotification, duracao) == 12, "PendingNotification: offset de duracao alterado");
static_assert(offsetof(PendingNotification, rssi) == 16, "PendingNotification: offset de rssi alterado");
static_assert(offsetof(PendingNotification, tipo) == 18, "PendingNotification: offset de tipo alterado");
static_assert(offsetof(PendingNotification, link) == 19, "PendingNotification: offset de link alterado");
static_assert(offsetof(PendingNotification, motivo) == 20, "PendingNotification: offset de motivo alterado");
static_assert(offsetof(PendingNotification, bootReason) == 21, "PendingNotification: offset de bootReason alterado");
static_assert(offsetof(PendingNotification, pending) == 22, "PendingNotification: offset de pending alterado");
static_assert(offsetof(PendingNotification, resetReasonRaw) == 23, "PendingNotification: resetReasonRaw fora do padding final");

static_assert(sizeof(LinkState) == 36, "LinkState: layout persistido alterado");
static_assert(sizeof(StorageHeader) == 8, "StorageHeader: layout persistido alterado");
static_assert(sizeof(RuntimeData) == 236, "RuntimeData: tamanho persistido alterado");
static_assert(offsetof(RuntimeData, bootReason) == 28, "RuntimeData: offset de bootReason alterado");
static_assert(offsetof(RuntimeData, links) == 32, "RuntimeData: offset de links alterado");
static_assert(offsetof(RuntimeData, pendingNotifications) == 140, "RuntimeData: offset de pendingNotifications alterado");
static_assert(sizeof(EventsData) == 6164, "EventsData: tamanho persistido alterado");
static_assert(offsetof(EventsData, events) == 20, "EventsData: offset de events alterado");

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

    String temp = String(file) + ".tmp";

    File f = LittleFS.open(temp.c_str(), "w");

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
        LittleFS.remove(temp.c_str());
        return false;
    }

    if (!LittleFS.rename(temp.c_str(), file)) {
        DBG("Não foi possível substituir %s\n", file);
        LittleFS.remove(temp.c_str());
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