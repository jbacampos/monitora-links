#ifndef NOTIFY_H
#define NOTIFY_H

#include <time.h>

#include "core/types.h"

//=============================================================================
// Fila de notificações
//=============================================================================

void queueNotification(const PendingNotification &n);

bool hasPendingNotifications();
uint8_t getPendingNotificationCount();
void sendPendingNotifications();

//=============================================================================
// Política de notificações
//=============================================================================

bool quietHoursEnabled();

bool inQuietHours(time_t t = 0);

void checkNotificationPolicy();

#endif