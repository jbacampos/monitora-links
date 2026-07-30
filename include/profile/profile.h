#ifndef PROFILES_H
#define PROFILES_H

#include "core/types.h"

extern const Perfil perfis[];
extern const uint8_t numPerfis;

const Perfil* detectProfile();
 
#endif