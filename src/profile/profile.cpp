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
                                    .identificaLocal = true } };

const LinkConfig linksSitio[] = { { .nome = "REDE SÍTIO",
                                    .ssid = "Sitio",
                                    .senha = SITIO_PASSWORD,
                                    .identificaLocal = true },
                                  { .nome = "REDE IOT",
                                    .ssid = "DispIoT",
                                    .senha = IOT_PASSWORD,
                                    .identificaLocal = false },
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
    
   // DBG("Detectando perfil. Total de redes encontradas: %d\n", total);
   // for (int i = 0; i < total; i++) {
   //    DBG("Rede %d: %s\n", i + 1, WiFi.SSID(i).c_str());
   // }

   for (uint8_t p = 0; p < numPerfis; p++) {
      bool perfilEncontrado = false;

      for (uint8_t s = 0; s < perfis[p].numLinks; s++) {
         // DBG("Verificando perfil %s, link %s. Identifica local: %s\n", perfis[p].nome, perfis[p].links[s].ssid, perfis[p].links[s].identificaLocal ? "SIM" : "NÃO"   );
         if (!perfis[p].links[s].identificaLocal){
            // DBG("Link não identifica local: %s\n", perfis[p].links[s].ssid);
            continue;
         }

         DBG("Link identifica local: %s. Vai verificar se está disponível...\n", perfis[p].links[s].ssid);

         for (int i = 0; i < total; i++) {
            if (WiFi.SSID(i) == perfis[p].links[s].ssid) {
               // DBG("Sinal encontrado: %s\n", WiFi.SSID(i).c_str());
               perfilEncontrado = true;
               break;
            }
         }

         if (perfilEncontrado) {
            break;
         }
      }

      if (perfilEncontrado)
         return &perfis[p];
   }

   return nullptr;
}