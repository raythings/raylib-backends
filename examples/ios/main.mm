#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include "raylib.h"
#include "rlgl.h"
#include "cube_scene.h"

extern "C" void RcoreIosMetal_SetLayer(void *metalLayer, int widthPx, int heightPx, float scale);
extern "C" void RcoreIosMetal_ResizeLayer(int widthPx, int heightPx, float scale);

@interface RaylibMetalView : UIView
@end

@implementation RaylibMetalView
+ (Class)layerClass
{
    return [CAMetalLayer class];
}
@end

@interface RaylibViewController : UIViewController
@property(nonatomic, strong) CADisplayLink *displayLink;
@property(nonatomic, assign) BOOL raylibReady;
@end

@implementation RaylibViewController

- (void)loadView
{
    self.view = [[RaylibMetalView alloc] initWithFrame:UIScreen.mainScreen.bounds];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.view.backgroundColor = UIColor.blackColor;
    CAMetalLayer *layer = (CAMetalLayer *)self.view.layer;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO;
    layer.contentsScale = UIScreen.mainScreen.scale;

    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(drawFrame)];
    [self.displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}

- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];

    CAMetalLayer *layer = (CAMetalLayer *)self.view.layer;
    CGFloat scale = self.view.window.screen.scale ?: UIScreen.mainScreen.scale;
    CGSize size = self.view.bounds.size;
    int widthPx = (int)lrint(size.width*scale);
    int heightPx = (int)lrint(size.height*scale);

    layer.contentsScale = scale;
    layer.drawableSize = CGSizeMake(widthPx, heightPx);
    RcoreIosMetal_SetLayer((__bridge void *)layer, widthPx, heightPx, (float)scale);

    if (self.raylibReady) RcoreIosMetal_ResizeLayer(widthPx, heightPx, (float)scale);
}

- (void)drawFrame
{
    if (!self.raylibReady)
    {
        CGSize size = self.view.bounds.size;
        if (size.width <= 0.0 || size.height <= 0.0) return;

        InitWindow((int)size.width, (int)size.height, "raylib-backends iOS Metal cube");
        SetTargetFPS(60);

        self.raylibReady = YES;
    }

    BeginDrawing();
        ClearBackground((Color){ 20, 28, 34, 255 });
        RaylibBackendsDrawCubeScene();
        DrawText("raylib-backends: iOS Metal", 24, 32, 20, RAYWHITE);
    EndDrawing();
}

@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@end

@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [RaylibViewController new];
    [self.window makeKeyAndVisible];
    return YES;
}
@end

int main(int argc, char *argv[])
{
    @autoreleasepool
    {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass(AppDelegate.class));
    }
}
