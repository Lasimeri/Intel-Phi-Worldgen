package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.gen.layer.GenLayer;
import net.minecraft.world.gen.layer.GenLayerRiverMix;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** GenLayerRiverMix's two inputs (it has no parent). See AccessorGenLayer.md. */
@Mixin(GenLayerRiverMix.class)
public interface AccessorGenLayerRiverMix {

    @Accessor("biomePatternGeneratorChain")
    GenLayer phiwg$getBiomes();

    @Accessor("riverPatternGeneratorChain")
    GenLayer phiwg$getRivers();
}
