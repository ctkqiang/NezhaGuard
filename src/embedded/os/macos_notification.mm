//
// Created by 钟智强 on 2026/8/7.
//

#import <UserNotifications/UserNotifications.h>

#include "macos_notification.h"

namespace Nezha::Service {

void macos_request_notification_permission() {
    [[UNUserNotificationCenter currentNotificationCenter]
        requestAuthorizationWithOptions:UNAuthorizationOptionAlert |
                                        UNAuthorizationOptionSound |
                                        UNAuthorizationOptionBadge
                      completionHandler:^(BOOL granted, NSError *error) {
                          if (granted) {
                              NSLog(@"[NezhaGuard] 通知权限已授权");
                          } else if (error) {
                              NSLog(@"[NezhaGuard] 通知权限请求失败: %@",
                                    error.localizedDescription);
                          }
                      }];
}

void macos_send_notification(const std::string &title, const std::string &body) {
    NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
    NSString *nsBody = [NSString stringWithUTF8String:body.c_str()];

    UNMutableNotificationContent *content = [[UNMutableNotificationContent alloc] init];
    content.title = nsTitle;
    content.body = nsBody;
    content.sound = [UNNotificationSound defaultSound];

    // deliver after 0.1s delay to ensure it appears even when app is foreground
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
}

} // namespace Nezha::Service