//
// Created by 钟智强 on 2026/8/7.
//

#import <UserNotifications/UserNotifications.h>

#include "macos_notification.h"

#include <cstdlib>
#include <memory>
#include <string>

static bool g_native_ok = false;

namespace Nezha::Service {

void macos_request_notification_permission() {
    // check if UserNotifications is available at runtime
    auto *center = [UNUserNotificationCenter currentNotificationCenter];
    if (!center) {
        NSLog(@"[NezhaGuard] UNUserNotificationCenter 不可用，回退到 osascript");
        return;
    }

    [center requestAuthorizationWithOptions:UNAuthorizationOptionAlert |
                                            UNAuthorizationOptionSound |
                                            UNAuthorizationOptionBadge
                          completionHandler:^(BOOL granted, NSError *error) {
                              if (granted) {
                                  g_native_ok = true;
                                  NSLog(@"[NezhaGuard] 原生通知已授权");
                              } else {
                                  // error 1 = not allowed in dev builds; fall back
                                  NSLog(@"[NezhaGuard] 原生通知不可用 (code=%ld)，回退到 osascript",
                                        (long)(error ? error.code : 0));
                              }
                          }];
}

void macos_send_notification(const std::string &title, const std::string &body) {
    // try native first
    if (g_native_ok) {
        NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
        NSString *nsBody = [NSString stringWithUTF8String:body.c_str()];

        UNMutableNotificationContent *content = [[UNMutableNotificationContent alloc] init];
        content.title = nsTitle;
        content.body = nsBody;
        content.sound = [UNNotificationSound defaultSound];

        UNTimeIntervalNotificationTrigger *trigger =
            [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:0.1 repeats:NO];

        UNNotificationRequest *request =
            [UNNotificationRequest requestWithIdentifier:[[NSUUID UUID] UUIDString]
                                                 content:content
                                                 trigger:trigger];

        [[UNUserNotificationCenter currentNotificationCenter]
            addNotificationRequest:request
             withCompletionHandler:^(NSError *error) {
                 if (error) {
                     NSLog(@"[NezhaGuard] 通知发送失败: %@", error.localizedDescription);
                 }
             }];
        return;
    }

    // fallback: osascript
    std::string escaped_body = body;
    std::string escaped_title = title;
    for (auto &s : {&escaped_body, &escaped_title}) {
        size_t pos = 0;
        while ((pos = s->find('"', pos)) != std::string::npos) {
            s->replace(pos, 1, "\\\"");
            pos += 2;
        }
        pos = 0;
        while ((pos = s->find('\n', pos)) != std::string::npos) {
            s->replace(pos, 1, " ");
        }
        pos = 0;
        while ((pos = s->find('\r', pos)) != std::string::npos) {
            s->erase(pos, 1);
        }
    }

    std::string cmd = "osascript -e 'display notification \"" + escaped_body +
                      "\" with title \"" + escaped_title +
                      "\" sound name \"Glass\"' 2>/dev/null";
    system(cmd.c_str());
}

} // namespace Nezha::Service