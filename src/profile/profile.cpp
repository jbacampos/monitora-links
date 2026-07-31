#include "profile/profile.h"
#include "config/config.h"
#include "config/platform.h"
#include "network/wifi_manager.h"

const LinkConfig linksAP903[] = { { .nome = "LINK VIVO",
                                    .ssid = "Bavi_Vivo_2G",
                                    .senha = VIVO_PASSWORD,
                                    .identificaLocal = true },
                                  { .nome = "LINK CLARO",
                                    .ssid = "Bavi_NET_2G",
                                    .senha = CLARO_PASSWORD,
                                    .identificaLocal = true },
                                  { .nome = "REDE AP_903",
                                    .ssid = "AP_903",
                                    .senha = AP_903_PASSWORD,
                                    .identificaLocal = false } };

const LinkConfig linksSitio[] = { { .nome = "REDE SÍTIO",
                                    .ssid = "Sitio",
                                    .senha = SITIO_PASSWORD,
                                    .identificaLocal = true },
                                  { .nome = "REDE IOT",
                                    .ssid = "DispIoT",
                                    .senha = IOT_PASSWORD,
                                    .identificaLocal = true },
                                 //  { .nome = "REDE CASINHA",
                                 //    .ssid = "Casinha",
                                 //    .senha = CASINHA_PASSWORD,
                                 //    .identificaLocal = false } 
                                 };

const TelegramConfig telegramAP903 = { TLGRM_TOKEN_AP903, TLGRM_CHAT_ID_AP903 };

const TelegramConfig telegramSitio = { TLGRM_TOKEN_SITIO, TLGRM_CHAT_ID_SITIO };

const Perfil perfis[] = { { "AP 903",
                            sizeof(linksAP903) / sizeof(linksAP903[0]),
                            linksAP903,
                            &telegramAP903 },
                          { "Sítio",
                            sizeof(linksSitio) / sizeof(linksSitio[0]),
                            linksSitio,
                            &telegramSitio } };

const uint8_t numPerfis = sizeof(perfis) / sizeof(perfis[0]);

const Perfil *gPerfil = nullptr;

const Perfil *detectProfile() {
   int total = WiFi.scanNetworks();

   for (uint8_t p = 0; p < numPerfis; p++) {
      bool perfilEncontrado = true;

      for (uint8_t s = 0; s < perfis[p].numLinks; s++) {
         if (!perfis[p].links[s].identificaLocal)
            continue;

         bool encontrou = false;

         for (int i = 0; i < total; i++) {
            if (WiFi.SSID(i) == perfis[p].links[s].ssid) {
               encontrou = true;
               break;
            }
         }

         if (!encontrou) {
            perfilEncontrado = false;
            break;
         }
      }

      if (perfilEncontrado)
         return &perfis[p];
   }

   return nullptr;
}