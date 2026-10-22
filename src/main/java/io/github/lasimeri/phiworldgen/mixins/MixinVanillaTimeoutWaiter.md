# MixinVanillaTimeoutWaiter.java

Turns Forge 1.7.10's 10-hour wait on a vanilla client into 2 seconds.

After login, `NetworkDispatcher.serverInitiateHandshake` sends FML's
handshake and adds `VanillaTimeoutWaiter` to the channel, scheduled with
`schedule(callable, 10, TimeUnit.HOURS)`. You can see this in the decompiled
`NetworkDispatcher` and in the class's bytecode: `ldc2_w 10l`, then
`getstatic TimeUnit.HOURS`.

A vanilla client never answers the handshake. So it sits at "Logging in"
for those 10 hours, while the server log fills with "Unexpected packet
during modded negotiation - assuming vanilla or keepalives :
C00PacketKeepAlive". This was seen on 2026-10-22 with the user's vanilla
client, in `~/.cache/phiworldgen/play.log`.

When the waiter fires, FML 1.7.10 does not let the client in. It kicks it:

```
Timeout occurred waiting for response, assuming vanilla connection
Timeout occurred, assuming a vanilla client
<name> lost connection: This is modded. No modded response received. Bye!
```

Measured with a minimal protocol-5 login (`mclogin.c` in the session
scratchpad) after this mixin. So a Forge 1.7.10 server cannot be joined
with a vanilla client at all. The mixin changes only how long that takes:
2 seconds and a clear message, instead of 10 hours at "Logging in".

A Forge client answers the handshake within milliseconds and completes as
modded, so it is unaffected. FML's classes are not obfuscated, so the target
and the injections are `remap = false`.
