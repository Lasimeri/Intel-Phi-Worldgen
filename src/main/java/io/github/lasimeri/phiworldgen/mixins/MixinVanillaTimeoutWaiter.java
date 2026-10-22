package io.github.lasimeri.phiworldgen.mixins;

import java.util.concurrent.TimeUnit;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.ModifyArg;

/** Lets a vanilla 1.7.10 client in after 2 s instead of FML's 10 hours. See MixinVanillaTimeoutWaiter.md. */
@Mixin(targets = "cpw.mods.fml.common.network.handshake.NetworkDispatcher$VanillaTimeoutWaiter", remap = false)
public abstract class MixinVanillaTimeoutWaiter {

    private static final String SCHEDULE = "Lio/netty/util/concurrent/EventExecutor;schedule(Ljava/util/concurrent/Callable;JLjava/util/concurrent/TimeUnit;)Lio/netty/util/concurrent/ScheduledFuture;";

    @ModifyArg(method = "handlerAdded", at = @At(value = "INVOKE", target = SCHEDULE), index = 1, remap = false)
    private long phiwg$delay(long delay) {
        return 2L;
    }

    @ModifyArg(method = "handlerAdded", at = @At(value = "INVOKE", target = SCHEDULE), index = 2, remap = false)
    private TimeUnit phiwg$unit(TimeUnit unit) {
        return TimeUnit.SECONDS;
    }
}
