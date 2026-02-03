# macosvm GDB Debug Stub

## Overview

Integration of the private `_VZGDBDebugStubConfiguration` API from Apple's Virtualization.framework into the macosvm project. This allows attaching a GDB/LLDB debugger to a running virtual machine at the hardware level, enabling kernel debugging of macOS/Linux guests.

The API was reverse-engineered from `dyld_shared_cache_arm64e` on macOS 26.2 using IDA Pro.

---

## Quick Start

```bash
# 1. Build
make clean && make

# 2. Create a VM (restore macOS from IPSW)
./macosvm --disk disk.img,size=40g --aux aux.img \
          --restore UniversalMac_26.2_25C56_Restore.ipsw vm.json

# 3. Run with GDB stub enabled
./macosvm -g --gdb 1234 vm.json

# 4. In another terminal, connect the debugger
lldb -o "gdb-remote localhost:1234"
```

---

## CLI Usage

```
--gdb <port>[,all]
```

| Parameter | Description |
|-----------|-------------|
| `<port>` | TCP port for the GDB server (required, e.g. `1234`) |
| `,all` | Listen on all network interfaces (`0.0.0.0`). Default: localhost only (`127.0.0.1`) |

### Examples

```bash
# Localhost only (127.0.0.1:1234)
./macosvm -g --gdb 1234 vm.json

# All interfaces (0.0.0.0:5555) - for remote debugging
./macosvm -g --gdb 5555,all vm.json

# Headless with GDB (no GUI)
./macosvm --gdb 1234 vm.json
```

### Notes

- `--gdb` is a **runtime-only** option. It is not saved to the JSON config.
- `--gdb` is **automatically skipped** during `--restore` (macOS installation). The debug stub is incompatible with the installer process. Apply `--gdb` on subsequent normal boots.

---

## Connecting a Debugger

### LLDB

```bash
lldb -o "gdb-remote localhost:1234"
```

Or interactively:

```
(lldb) gdb-remote localhost:1234
(lldb) register read
(lldb) memory read 0xfffffe0007004000
(lldb) disassemble --pc
```

### GDB

```bash
gdb -ex "target remote localhost:1234"
```

### Remote Debugging

If `--gdb <port>,all` was used, connect from another machine:

```bash
lldb -o "gdb-remote <host-ip>:1234"
```

---

## Architecture

### Private API Classes

Discovered via reverse engineering of Virtualization.framework:

```
_VZDebugStubConfiguration          (base class)
  +-- _VZGDBDebugStubConfiguration (GDB implementation)
        - initWithPort:(unsigned short)port
        - port
        - listensOnAllNetworkInterfaces

_VZDebugStub                       (base class)
  +-- _VZGDBDebugStub              (GDB implementation)
        - delegate
        - port

VZVirtualMachineConfiguration (VZPrivate)
  - _setDebugStub:                 (attach config BEFORE creating VM)
  - _debugStub                     (retrieve config)

VZVirtualMachine (VZPrivate)
  - _debugStub                     (retrieve active stub after VM start)
```

### Internal Flow

```
                     +--------------------------+
                     | _VZGDBDebugStubConfig    |
                     | port=1234, listenAll=NO  |
                     +-----------+--------------+
                                 |
                    _setDebugStub: (before VM creation)
                                 |
                     +-----------v--------------+
                     | VZVirtualMachineConfig   |
                     | _debugStub = config      |
                     +-----------+--------------+
                                 |
                   initWithConfiguration:queue:
                                 |
                     +-----------v--------------+
                     | VZVirtualMachine         |
                     +-----------+--------------+
                                 |
                      startWithOptions: ...
                                 |
              +------------------v-------------------+
              | _synchronousConfigurationForVM:      |
              |   1. [vm _debugStub]                 |
              |      -> makeDebugStubForVM:          |
              |      -> creates _VZGDBDebugStub      |
              |   2. [gdbStub _debugStub]            |
              |      -> socket(AF_INET, SOCK_STREAM) |
              |      -> setsockopt(SO_REUSEADDR)     |
              |      -> bind(port)                   |
              |      -> listen()                     |
              |   3. server_socket_fd -> XPC -> VMM  |
              +------------------+-------------------+
                                 |
                     +-----------v--------------+
                     | VMM (XPC service)        |
                     | com.apple.Virtualization |
                     | .VirtualMachine          |
                     | receives fd, serves GDB  |
                     +--------------------------+
```

The socket is created in the host process and the file descriptor is passed to the VMM XPC service via configuration serialization. The VMM process then handles the GDB protocol over that socket.

### Delegate

After a successful VM start, the `_VZGDBDebugStubDelegate` receives:

```objc
- (void)_debugStub:(id)stub didStartListeningOnPort:(unsigned short)port;
```

This is used to print the connection banner in the console.

---

## Required Entitlements

```xml
<!-- Standard virtualization -->
<key>com.apple.security.virtualization</key>
<true/>

<!-- REQUIRED for GDB stub - private entitlement -->
<key>com.apple.private.virtualization</key>
<true/>

<!-- Network permissions for socket binding -->
<key>com.apple.security.network.server</key>
<true/>
```

The `com.apple.private.virtualization` entitlement is **critical**. Without it, the VMM XPC service rejects the debug stub configuration and the VM fails to start with `VZErrorDomain Code=1 "Internal Virtualization error"`.

This entitlement requires either:
- An ad-hoc signature with SIP disabled, or
- A proper Apple Developer certificate with the private entitlement provisioned

---

## Files Modified

| File | Changes |
|------|---------|
| `VMInstance.h` | Private API declarations (`_VZGDBDebugStubConfiguration`, `_VZGDBDebugStub`, delegate protocol, `VZVirtualMachineConfiguration`/`VZVirtualMachine` private categories). Added `gdb_port` and `gdb_listen_all` fields to `VMSpec`. |
| `VMInstance.m` | `GDBStubDelegate` implementation. GDB stub setup in `configure` method (skipped during restore). Delegate attachment after VM start in `start_`. |
| `main.m` | `--gdb <port>[,all]` CLI option parsing and help text. |
| `macosvm.entitlements` | Added `com.apple.private.virtualization`. |

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `VZErrorDomain Code=1 "Internal Virtualization error"` at start | Missing `com.apple.private.virtualization` entitlement. Ensure SIP is disabled for ad-hoc signing. |
| `VZErrorDomain Code=10007` during `--restore` | Do not use `--gdb` with `--restore`. Install macOS first, then run with `--gdb`. |
| `bind: Address already in use` | Another process is using the port. Choose a different port or kill the conflicting process. |
| Debugger cannot connect | Verify the VM has fully booted. Check firewall. Use `,all` if connecting remotely. |
| VM starts but no GDB banner | The delegate fires after VM start. Wait for boot to complete. Check the console output. |
