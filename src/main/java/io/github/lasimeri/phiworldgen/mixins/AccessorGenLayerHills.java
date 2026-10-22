package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.gen.layer.GenLayer;
import net.minecraft.world.gen.layer.GenLayerHills;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** GenLayerHills' second input, the zoomed river-init noise. See AccessorGenLayer.md. */
@Mixin(GenLayerHills.class)
public interface AccessorGenLayerHills {

    @Accessor("field_151628_d")
    GenLayer phiwg$getRiverNoise();
}
