package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.gen.layer.GenLayer;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** A GenLayer's parent and its two seeds, for the layer capture. See AccessorGenLayer.md. */
@Mixin(GenLayer.class)
public interface AccessorGenLayer {

    @Accessor("parent")
    GenLayer phiwg$getParent();

    @Accessor("baseSeed")
    long phiwg$getBaseSeed();

    @Accessor("worldGenSeed")
    long phiwg$getWorldGenSeed();
}
