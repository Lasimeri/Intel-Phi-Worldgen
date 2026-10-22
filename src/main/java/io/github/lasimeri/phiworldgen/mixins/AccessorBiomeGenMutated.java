package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.biome.BiomeGenBase;
import net.minecraft.world.biome.BiomeGenMutated;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** The biome a mutated biome hands its surface to. See AccessorBiomeGenHills.md. */
@Mixin(BiomeGenMutated.class)
public interface AccessorBiomeGenMutated {

    @Accessor("baseBiome")
    BiomeGenBase phiwg$getBase();
}
