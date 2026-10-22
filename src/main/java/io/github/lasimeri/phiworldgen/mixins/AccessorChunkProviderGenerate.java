package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.WorldType;
import net.minecraft.world.biome.BiomeGenBase;
import net.minecraft.world.gen.ChunkProviderGenerate;
import net.minecraft.world.gen.MapGenBase;
import net.minecraft.world.gen.NoiseGeneratorOctaves;
import net.minecraft.world.gen.NoiseGeneratorPerlin;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;
import org.spongepowered.asm.mixin.gen.Invoker;

/** The overworld generator's private state and its noise-field function. See AccessorChunkProviderGenerate.md. */
@Mixin(ChunkProviderGenerate.class)
public interface AccessorChunkProviderGenerate {

    @Accessor("field_147431_j")
    NoiseGeneratorOctaves phiwg$getMinLimit();

    @Accessor("field_147432_k")
    NoiseGeneratorOctaves phiwg$getMaxLimit();

    @Accessor("field_147429_l")
    NoiseGeneratorOctaves phiwg$getMain();

    @Accessor("field_147430_m")
    NoiseGeneratorPerlin phiwg$getSurfaceNoise();

    @Accessor("parabolicField")
    float[] phiwg$getParabolicField();

    @Accessor("biomesForGeneration")
    BiomeGenBase[] phiwg$getBiomes();

    @Accessor("biomesForGeneration")
    void phiwg$setBiomes(BiomeGenBase[] biomes);

    @Accessor("field_147426_g")
    double[] phiwg$getDepthNoise();

    @Accessor("field_147427_d")
    double[] phiwg$getMainNoise();

    @Accessor("field_147428_e")
    double[] phiwg$getMinLimitNoise();

    @Accessor("field_147425_f")
    double[] phiwg$getMaxLimitNoise();

    @Accessor("field_147434_q")
    double[] phiwg$getField();

    @Accessor("field_147435_p")
    WorldType phiwg$getWorldType();

    @Accessor("mapFeaturesEnabled")
    boolean phiwg$getMapFeatures();

    @Accessor("caveGenerator")
    MapGenBase phiwg$getCaves();

    @Accessor("ravineGenerator")
    MapGenBase phiwg$getRavines();

    @Invoker("func_147423_a")
    void phiwg$noiseField(int x, int y, int z);
}
