package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.biome.BiomeGenHills;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** BiomeGenHills' three variant numbers, which pick its surface (gravel, stone). See AccessorBiomeGenHills.md. */
@Mixin(BiomeGenHills.class)
public interface AccessorBiomeGenHills {

    @Accessor("field_150636_aF")
    int phiwg$getAF();

    @Accessor("field_150637_aG")
    int phiwg$getAG();

    @Accessor("field_150638_aH")
    int phiwg$getAH();
}
