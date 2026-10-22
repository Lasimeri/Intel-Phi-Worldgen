package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.biome.BiomeGenTaiga;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** BiomeGenTaiga's type: 1 and 2 (the mega taigas) rewrite their surface per column. See AccessorBiomeGenHills.md. */
@Mixin(BiomeGenTaiga.class)
public interface AccessorBiomeGenTaiga {

    @Accessor("field_150644_aH")
    int phiwg$getType();
}
