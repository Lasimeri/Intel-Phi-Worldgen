package io.github.lasimeri.phiworldgen.mixins;

import java.util.List;

import net.minecraft.world.gen.layer.GenLayerBiome;
import net.minecraftforge.common.BiomeManager.BiomeEntry;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;

/** GenLayerBiome's weighted lists per BiomeType, as its constructor built them (a Forge field). See AccessorGenLayer.md. */
@Mixin(GenLayerBiome.class)
public interface AccessorGenLayerBiome {

    @Accessor(value = "biomes", remap = false)
    List<BiomeEntry>[] phiwg$getBiomes();
}
