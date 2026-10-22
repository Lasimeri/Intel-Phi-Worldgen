package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.gen.NoiseGeneratorImproved;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** The permutation table of one Perlin octave. See AccessorNoiseGeneratorImproved.md. */
@Mixin(NoiseGeneratorImproved.class)
public interface AccessorNoiseGeneratorImproved {

    @Accessor("permutations")
    int[] phiwg$getPermutations();

    @Accessor("permutations")
    void phiwg$setPermutations(int[] permutations);
}
