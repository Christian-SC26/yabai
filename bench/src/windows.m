//
// NOTE: the windows the live benchmark tiles, swaps and closes, so that it
// never has to touch a window the user is working in. It opens the given
// number of plain titled windows, prints their ids, and on every line read
// from stdin acts on it: "open" opens one more window, "close <id>" closes
// one, "quit" exits. Closing stdin also exits.
//

#import <Cocoa/Cocoa.h>

static NSMutableArray *g_windows;

static void open_window(void)
{
    static int counter;
    NSRect frame = NSMakeRect(100 + 20 * (counter % 10), 100 + 20 * (counter % 10), 420, 300);
    NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
                                                   styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    [window setTitle:[NSString stringWithFormat:@"yabai-bench-%d", ++counter]];
    [window setReleasedWhenClosed:NO];
    [window makeKeyAndOrderFront:nil];
    [g_windows addObject:window];
    printf("window %ld\n", (long) [window windowNumber]);
    fflush(stdout);
}

static void handle_line(NSString *line)
{
    NSArray *parts = [line componentsSeparatedByString:@" "];
    NSString *command = parts.count ? parts[0] : @"";

    if ([command isEqualToString:@"open"]) {
        open_window();
    } else if ([command isEqualToString:@"close"] && parts.count > 1) {
        long wid = [parts[1] integerValue];
        for (NSWindow *window in [g_windows copy]) {
            if ([window windowNumber] == wid) {
                [window close];
                [g_windows removeObject:window];
                break;
            }
        }
        printf("closed %ld\n", wid);
        fflush(stdout);
    } else if ([command isEqualToString:@"quit"]) {
        exit(0);
    }
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        int count = argc > 1 ? atoi(argv[1]) : 4;
        g_windows = [NSMutableArray array];

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        dispatch_async(dispatch_get_main_queue(), ^{
            [NSApp activateIgnoringOtherApps:YES];
            for (int i = 0; i < count; ++i) open_window();
            printf("ready\n");
            fflush(stdout);
        });

        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
            char buf[256];
            while (fgets(buf, sizeof(buf), stdin)) {
                NSString *line = [[NSString stringWithUTF8String:buf] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
                dispatch_sync(dispatch_get_main_queue(), ^{ handle_line(line); });
            }
            exit(0);
        });

        [NSApp run];
    }
    return 0;
}
