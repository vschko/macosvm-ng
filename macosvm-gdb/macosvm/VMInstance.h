#import <Foundation/Foundation.h>
#import <Virtualization/Virtualization.h>

#ifdef __arm64__
#define MACOS_GUEST 1
#endif

#pragma mark - Private GDB Debug Stub API (Virtualization.framework)

@class _VZGDBDebugStub;

@protocol _VZGDBDebugStubDelegate <NSObject>
@optional
- (void)_debugStub:(id)stub didStartListeningOnPort:(unsigned short)port;
@end

@interface _VZDebugStubConfiguration : NSObject <NSCopying>
- (instancetype)_init;
- (id)makeDebugStubForVirtualMachine:(VZVirtualMachine *)vm;
@end

@interface _VZGDBDebugStubConfiguration : _VZDebugStubConfiguration
- (instancetype)initWithPort:(unsigned short)port;
@property (nonatomic, assign) unsigned short port;
@property (nonatomic, assign) BOOL listensOnAllNetworkInterfaces;
@end

@interface _VZDebugStub : NSObject
@end

@interface _VZGDBDebugStub : _VZDebugStub
@property (nonatomic, weak) id<_VZGDBDebugStubDelegate> delegate;
@property (nonatomic, readonly) unsigned short port;
@end

@interface VZVirtualMachineConfiguration (VZPrivate)
- (void)_setDebugStub:(_VZDebugStubConfiguration *)stub;
- (nullable _VZDebugStubConfiguration *)_debugStub;
@end

@interface VZVirtualMachine (VZPrivate)
- (nullable _VZDebugStub *)_debugStub;
@end

@interface VZVirtualMachineStartOptions (VZPrivate)
- (void)_setDelegatedExceptionClasses:(NSArray<NSNumber *> * _Nonnull)classes;
@end

#ifdef MACOS_GUEST
@interface _VZMacHardwareModelDescriptor : NSObject
- (void)setPlatformVersion:(uint32_t)version;
- (void)setBoardID:(uint32_t)boardID;
- (void)setISA:(int64_t)isa;
- (void)setVariantID:(unsigned int)variantID variantName:(NSString * _Nonnull)variantName;
@end

@interface VZMacHardwareModel (VZPrivate)
+ (instancetype _Nullable)_hardwareModelWithDescriptor:(_VZMacHardwareModelDescriptor * _Nonnull)descriptor;
- (unsigned int)_boardID;
- (long long)_isa;
- (unsigned int)_variantID;
- (NSString * _Nullable)_variantName;
@end

@interface VZMacAuxiliaryStorage (VZPrivate)
- (BOOL)_setDataValue:(NSData * _Nonnull)value
forNVRAMVariableNamed:(NSString * _Nonnull)name
                error:(NSError * _Nullable * _Nullable)error;
@end
#endif

@interface VMSpec : VZVirtualMachineConfiguration {
    NSData  *machineIdentifierData, *hardwareModelData;
    NSArray *storage;
    /* type: disk / aux / initrd
       file: / url:
       readOnly: true/false */
    NSArray *displays;
    /* width:  height:  dpi: */
    NSArray *networks;
    /* type:  mac:  interface: */
    NSArray *shares;
    /* */
    NSString *os;
    /* macos / linux */
    NSDictionary *bootInfo;
    /* Linux: kernel, parameters */
    NSString *bootArgs;
    /* macOS: boot-args NVRAM variable */
    NSDictionary *hardwareModelOverride;
    /* macOS: private hardware model descriptor override */

    NSString *ptyPath; /* internally generated */
@public
    int cpus;
    unsigned long ram;
    BOOL audio, use_serial, pty, spice, use_recovery;
    unsigned short gdb_port;
    BOOL gdb_listen_all;
    BOOL gdb_delegate_hvc;
    NSString *spawnScript;
}

#ifdef MACOS_GUEST
@property (strong) VZMacOSRestoreImage *restoreImage;
#endif

- (instancetype) init;
- (NSError *) readFromJSON: (NSInputStream*) jsonStream;
- (NSError*) writeToJSON: (NSOutputStream*) jsonStream;
- (void) addFileStorage: (NSString*) path type: (NSString*) type readOnly: (BOOL) ro;
- (void) addFileStorage: (NSString*) path type: (NSString*) type options: (NSArray*) options;
- (void) addDefaults;
- (void) addDisplayWithWidth: (int) width height: (int) height dpi: (int) dpi;
- (void) addNetwork: (NSString*) type;
- (void) addNetwork: (NSString*) type mac:(NSString*) mac;
- (void) addNetwork: (NSString*) type interface: (NSString*) iface;
- (void) addNetwork: (NSString*) type interface: (NSString*) iface mac: (NSString*) mac;
- (void) addNetworkSpecification: (NSDictionary*) spec;
- (void) addDirectoryShare: (NSString*) path volume: (NSString*) volume readOnly: (BOOL) readOnly;
- (void) addDirectoryShares: (NSArray*) paths volume: (NSString*) volume readOnly: (BOOL) readOnly;
- (void) addAutomountDirectoryShare: (NSString*) path readOnly: (BOOL) readOnly;
- (void) addAutomountDirectoryShares: (NSArray*) paths readOnly: (BOOL) readOnly;
- (void) setPrimaryMAC: (NSString*) mac;
- (void) setSpawnScript: (NSString*) mac;
- (void) setBootArgs: (NSString * _Nonnull) args;
- (BOOL) setHardwareModelSpecification: (NSString * _Nonnull) spec error: (NSError * _Nullable * _Nullable) err;
- (instancetype) configure;
- (void) cloneAllStorage;

@end

@interface VMInstance : NSObject
{
    @public
    dispatch_queue_t queue;
//    VZMacOSInstaller *installer;
}

@property (strong) VZVirtualMachine *virtualMachine;
@property (strong) VMSpec *spec;

- (instancetype) initWithSpec: (VMSpec*) spec;
- (void) start;
- (BOOL) stop;
- (void) performVM: (id) target selector: (SEL) aSelector withObject:(nullable id)anArgument;

@end
