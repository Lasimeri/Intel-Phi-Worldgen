package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.gen.MapGenBase;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** A carver's range in chunks (8 in vanilla): how far its source chunks reach. See AccessorMapGenBase.md. */
@Mixin(MapGenBase.class)
public interface AccessorMapGenBase {

    @Accessor("range")
    int phiwg$getRange();
}
