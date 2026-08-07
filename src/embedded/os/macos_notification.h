//
// Created by 钟智强 on 2026/8/7.
//

#ifndef NEZHAGUARD_MACOS_NOTIFICATION_H
#define NEZHAGUARD_MACOS_NOTIFICATION_H

#include <string>

namespace Nezha::Service {

// request notification permission (call once at app startup)
void macos_request_notification_permission();

// send a local notification through native macOS UserNotifications API
void macos_send_notification(const std::string &title, const std::string &body);

} // namespace Nezha::Service

#endif // NEZHAGUARD_MACOS_NOTIFICATION_H