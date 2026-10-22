package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.gen.NoiseGeneratorSimplex;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** A NoiseGeneratorSimplex's permutation table. See AccessorBiomeGenHills.md. */
@Mixin(NoiseGeneratorSimplex.class)
public interface AccessorNoiseGeneratorSimplex {

    @Accessor("field_151608_f")
    int[] phiwg$getPermutations();
}
