package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.biome.WorldChunkManager;
import net.minecraft.world.gen.layer.GenLayer;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** The two GenLayer chains of the overworld's chunk manager. See AccessorGenLayer.md. */
@Mixin(WorldChunkManager.class)
public interface AccessorWorldChunkManager {

    /** getBiomesForGeneration: the 10 x 10 biomes of the shape stage (GenLayerRiverMix). */
    @Accessor("genBiomes")
    GenLayer phiwg$getGenBiomes();

    /** getBiomeGenAt: the block biomes, 16 x 16 a chunk (GenLayerVoronoiZoom). */
    @Accessor("biomeIndexLayer")
    GenLayer phiwg$getBiomeIndexLayer();
}
