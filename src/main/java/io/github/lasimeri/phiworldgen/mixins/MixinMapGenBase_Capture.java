package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.block.Block;
import net.minecraft.world.World;
import net.minecraft.world.chunk.IChunkProvider;
import net.minecraft.world.gen.MapGenBase;
import net.minecraft.world.gen.MapGenCaves;
import net.minecraft.world.gen.MapGenRavine;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import io.github.lasimeri.phiworldgen.capture.Capture;

/** Records the block array after the caves and after the ravines. See MixinMapGenBase_Capture.md. */
@Mixin(MapGenBase.class)
public abstract class MixinMapGenBase_Capture {

    @Inject(method = "func_151539_a", at = @At("RETURN"))
    private void phiwg$capCarve(IChunkProvider provider, World world, int x, int z, Block[] blocks, CallbackInfo ci) {
        if (!Capture.on() || blocks == null) return;
        Class<?> c = ((Object) this).getClass();
        if (c == MapGenCaves.class) Capture.blocks("CAVE", blocks);
        else if (c == MapGenRavine.class) Capture.blocks("RAVN", blocks);
    }
}
