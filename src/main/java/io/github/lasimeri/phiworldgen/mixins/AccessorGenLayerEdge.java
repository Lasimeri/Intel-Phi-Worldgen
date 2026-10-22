package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.gen.layer.GenLayerEdge;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** GenLayerEdge's mode (COOL_WARM, HEAT_ICE, SPECIAL). See AccessorGenLayer.md. */
@Mixin(GenLayerEdge.class)
public interface AccessorGenLayerEdge {

    @Accessor("field_151627_c")
    GenLayerEdge.Mode phiwg$getMode();
}
