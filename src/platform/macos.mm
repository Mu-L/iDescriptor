#include "./macos.h"
#include <Cocoa/Cocoa.h>
#include <QDebug>
#include <QMainWindow>

void setupMacOSWindow(QMainWindow *window) {
    window->setUnifiedTitleAndToolBarOnMac(true);

    NSView* nativeView = reinterpret_cast<NSView*>(window->winId());
    NSWindow* nativeWindow = [nativeView window];

    [nativeWindow setStyleMask:[nativeWindow styleMask] | NSWindowStyleMaskFullSizeContentView | NSWindowTitleHidden];
    [nativeWindow setTitlebarAppearsTransparent:YES];
    [nativeWindow center];
}
// TODO:remove
void setupMacOSWindowOLD(QMainWindow *window)
{

    if (!window) {
        qWarning() << "setupMacOSWindow: window is null";
        return;
    }

    NSView *nativeView = reinterpret_cast<NSView *>(window->winId());
    NSWindow *nativeWindow = [nativeView window];
    [nativeWindow setMovableByWindowBackground:YES];
    if (!nativeWindow) {
        qWarning() << "setupMacOSWindow: native window is null";
        return;
    }
    // TODO: implement theme switching from app settings
    // // Force dark mode
    // nsWindow.overrideUserInterfaceStyle = NSUserInterfaceStyleDark;

    // Force light mode
    // if (@available(macOS 10.14, *)) {
    //     [nativeWindow
    //         setAppearance:[NSAppearance
    //         appearanceNamed:NSAppearanceNameAqua]];
    // } else {
    //     // Fallback: no-op on older macOS versions
    // }
    // nativeWindow.overrideUserInterfaceStyle = NSUserInterfaceStyleLight;

    // // Reset to follow system (default)
    // nsWindow.overrideUserInterfaceStyle = NSUserInterfaceStyleUnspecified;

    qDebug() << "Setting up macOS window styles";

    // window->setUnifiedTitleAndToolBarOnMac(true);

    [nativeWindow setStyleMask:[nativeWindow styleMask] |
                               NSWindowStyleMaskFullSizeContentView |
                               NSWindowTitleHidden];
    [nativeWindow setTitleVisibility:NSWindowTitleHidden];
    [nativeWindow setTitlebarAppearsTransparent:YES];

    NSToolbar *toolbar =
        [[NSToolbar alloc] initWithIdentifier:@"HiddenInsetToolbar"];
    toolbar.showsBaselineSeparator =
        NO; // equivalent to HideToolbarSeparator: true
    [nativeWindow setToolbar:toolbar];
    // [toolbar setVisible:NO];
    // todo : is it ok ?
    [toolbar release];
    // [nativeWindow setContentBorderThickness:0.0 forEdge:NSMinYEdge];

    [nativeWindow center];
}

@interface DiskUsagePopoverViewController : NSViewController
@property(nonatomic, strong) NSTextField *typeLabel;
@property(nonatomic, strong) NSTextField *sizeLabel;
@property(nonatomic, strong) NSTextField *percentageLabel;
@end

// Static variables for popover management
NSPopover *s_popover = nil;
NSViewController *s_viewController = nil;

@implementation DiskUsagePopoverViewController
- (void)loadView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 180, 80)];

    // Type label
    self.typeLabel =
        [[NSTextField alloc] initWithFrame:NSMakeRect(40, 55, 130, 16)];
    self.typeLabel.editable = NO;
    self.typeLabel.selectable = NO;
    self.typeLabel.bordered = NO;
    self.typeLabel.backgroundColor = [NSColor clearColor];
    self.typeLabel.font = [NSFont boldSystemFontOfSize:13];
    [view addSubview:self.typeLabel];

    // Size label
    self.sizeLabel =
        [[NSTextField alloc] initWithFrame:NSMakeRect(10, 30, 160, 16)];
    self.sizeLabel.editable = NO;
    self.sizeLabel.selectable = NO;
    self.sizeLabel.bordered = NO;
    self.sizeLabel.backgroundColor = [NSColor clearColor];
    self.sizeLabel.font = [NSFont systemFontOfSize:11];
    [view addSubview:self.sizeLabel];

    // Percentage label
    self.percentageLabel =
        [[NSTextField alloc] initWithFrame:NSMakeRect(10, 10, 160, 16)];
    self.percentageLabel.editable = NO;
    self.percentageLabel.selectable = NO;
    self.percentageLabel.bordered = NO;
    self.percentageLabel.backgroundColor = [NSColor clearColor];
    self.percentageLabel.font = [NSFont systemFontOfSize:11];
    self.percentageLabel.textColor = [NSColor secondaryLabelColor];
    [view addSubview:self.percentageLabel];

    self.view = view;
}

- (void)updateWithInfo:(const UsageInfo &)info
{
    self.typeLabel.stringValue =
        [NSString stringWithUTF8String:info.type.toUtf8().constData()];
    self.sizeLabel.stringValue =
        [NSString stringWithUTF8String:info.formattedSize.toUtf8().constData()];
    self.percentageLabel.stringValue = [NSString
        stringWithFormat:@"%.1f%% of total capacity", info.percentage];
}

@end

void hidePopoverForBarWidget()
{
    if (s_popover) {
        [s_popover close];
        [s_popover release];
        s_popover = nil;
    }
    if (s_viewController) {
        [s_viewController release];
        s_viewController = nil;
    }
}
// TODO: bug report to Qt, window becomes blurry, shifted or resized after
// showing popover
void showPopoverForBarWidget(QWidget *widget, const UsageInfo &info)
{
    if (!widget)
        return;

    // Hide existing popover if any
    hidePopoverForBarWidget();

    // Get the native view
    NSView *nativeView = reinterpret_cast<NSView *>(widget->winId());
    if (!nativeView)
        return;

    NSWindow *window = [nativeView window];
    if (!window)
        return;

    // Create view controller and force view loading
    DiskUsagePopoverViewController *viewController =
        [[DiskUsagePopoverViewController alloc] init];

    // Force the view to load before updating
    [viewController loadView];
    [viewController updateWithInfo:info];

    // Create popover
    NSPopover *popover = [[NSPopover alloc] init];
    [popover setContentSize:NSMakeSize(180, 80)];
    [popover setBehavior:NSPopoverBehaviorTransient];
    [popover setAnimates:YES];
    [popover setContentViewController:viewController];

    // Use the widget's bounds for a simpler approach
    NSRect widgetBounds = nativeView.bounds;

    // Show popover
    [popover showRelativeToRect:widgetBounds
                         ofView:nativeView
                  preferredEdge:NSMinYEdge];

    // Store references (retain them)
    s_popover = [popover retain];
    s_viewController = [viewController retain];
}
