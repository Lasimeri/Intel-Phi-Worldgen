package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.gen.NoiseGeneratorPerlin;
import net.minecraft.world.gen.NoiseGeneratorSimplex;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** The simplex octaves of a NoiseGeneratorPerlin (the surface's stone noise). See AccessorBiomeGenHills.md. */
@Mixin(NoiseGeneratorPerlin.class)
public interface AccessorNoiseGeneratorPerlin {

    @Accessor("field_151603_a")
    NoiseGeneratorSimplex[] phiwg$getOctaves();
}
