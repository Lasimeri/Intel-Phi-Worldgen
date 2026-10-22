package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.block.Block;
import net.minecraft.world.biome.BiomeGenBase;
import net.minecraft.world.gen.ChunkProviderGenerate;

import org.objectweb.asm.Opcodes;
import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import io.github.lasimeri.phiworldgen.capture.Capture;

/** Records each stage boundary of a chunk the capture is generating. See MixinChunkProviderGenerate_Capture.md. */
@Mixin(ChunkProviderGenerate.class)
public abstract class MixinChunkProviderGenerate_Capture {

    @Shadow
    private BiomeGenBase[] biomesForGeneration;
    @Shadow
    double[] field_147426_g;
    @Shadow
    double[] field_147427_d;
    @Shadow
    double[] field_147428_e;
    @Shadow
    double[] field_147425_f;
    @Shadow
    @Final
    private double[] field_147434_q;

    @Inject(
        method = "func_147424_a",
        at = @At(
            value = "FIELD",
            target = "Lnet/minecraft/world/gen/ChunkProviderGenerate;biomesForGeneration:[Lnet/minecraft/world/biome/BiomeGenBase;",
            opcode = Opcodes.PUTFIELD,
            shift = At.Shift.AFTER))
    private void phiwg$capBiomes10(int x, int z, Block[] blocks, CallbackInfo ci) {
        if (Capture.on()) Capture.biomes("BI10", biomesForGeneration, 100);
    }

    @Inject(
        method = "func_147423_a",
        at = @At(
            value = "FIELD",
            target = "Lnet/minecraft/world/gen/ChunkProviderGenerate;field_147425_f:[D",
            opcode = Opcodes.PUTFIELD,
            shift = At.Shift.AFTER))
    private void phiwg$capOctaves(CallbackInfo ci) {
        if (!Capture.on()) return;
        Capture.doubles("OCT0", field_147426_g, 25);
        Capture.doubles("OCT1", field_147427_d, 825);
        Capture.doubles("OCT2", field_147428_e, 825);
        Capture.doubles("OCT3", field_147425_f, 825);
    }

    @Inject(
        method = "func_147424_a",
        at = @At(
            value = "INVOKE",
            target = "Lnet/minecraft/world/gen/ChunkProviderGenerate;func_147423_a(III)V",
            shift = At.Shift.AFTER))
    private void phiwg$capField(int x, int z, Block[] blocks, CallbackInfo ci) {
        if (Capture.on()) Capture.doubles("FLD ", field_147434_q, 825);
    }

    @Inject(method = "func_147424_a", at = @At("RETURN"))
    private void phiwg$capShape(int x, int z, Block[] blocks, CallbackInfo ci) {
        if (Capture.on()) Capture.blocks("SHAP", blocks);
    }

    @Inject(method = "replaceBlocksForBiome", at = @At(value = "RETURN", ordinal = 0))
    private void phiwg$capSurfaceDenied(int x, int z, Block[] blocks, byte[] meta, BiomeGenBase[] biomes,
        CallbackInfo ci) {
        if (Capture.on()) Capture.flag(Capture.FLAG_SURFACE_DENIED);
    }

    @Inject(method = "replaceBlocksForBiome", at = @At("RETURN"))
    private void phiwg$capSurface(int x, int z, Block[] blocks, byte[] meta, BiomeGenBase[] biomes, CallbackInfo ci) {
        if (!Capture.on()) return;
        Capture.biomes("BI16", biomes, 256);
        Capture.blocks("SURF", blocks);
        Capture.bytes("META", meta);
        Capture.surfaceState();
    }
}
