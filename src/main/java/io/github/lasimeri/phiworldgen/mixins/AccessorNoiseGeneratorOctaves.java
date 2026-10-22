package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.gen.NoiseGeneratorImproved;
import net.minecraft.world.gen.NoiseGeneratorOctaves;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/**
 * The octaves of a NoiseGeneratorOctaves, for the capture and its verification. See AccessorNoiseGeneratorOctaves.md.
 */
@Mixin(NoiseGeneratorOctaves.class)
public interface AccessorNoiseGeneratorOctaves {

    @Accessor("generatorCollection")
    NoiseGeneratorImproved[] phiwg$getGenerators();
}
