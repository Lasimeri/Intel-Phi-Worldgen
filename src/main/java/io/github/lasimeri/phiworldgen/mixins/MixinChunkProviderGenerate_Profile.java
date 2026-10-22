package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.world.chunk.Chunk;
import net.minecraft.world.chunk.IChunkProvider;
import net.minecraft.world.gen.ChunkProviderGenerate;

import org.objectweb.asm.Opcodes;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.github.lasimeri.phiworldgen.profile.GenProfile;

/** Marks the phases of overworld generation for GenProfile. See MixinChunkProviderGenerate_Profile.md. */
@Mixin(ChunkProviderGenerate.class)
public abstract class MixinChunkProviderGenerate_Profile {

    private static final String BIOMES = "Lnet/minecraft/world/biome/WorldChunkManager;";
    private static final String GEN_BIOMES = BIOMES
        + "getBiomesForGeneration([Lnet/minecraft/world/biome/BiomeGenBase;IIII)[Lnet/minecraft/world/biome/BiomeGenBase;";
    private static final String CARVE = "Lnet/minecraft/world/gen/MapGenBase;func_151539_a(Lnet/minecraft/world/chunk/IChunkProvider;Lnet/minecraft/world/World;II[Lnet/minecraft/block/Block;)V";
    private static final String SURFACE = "Lnet/minecraft/world/gen/ChunkProviderGenerate;replaceBlocksForBiome(II[Lnet/minecraft/block/Block;[B[Lnet/minecraft/world/biome/BiomeGenBase;)V";
    private static final String NOISE = "Lnet/minecraft/world/gen/ChunkProviderGenerate;func_147423_a(III)V";
    private static final String SKYLIGHT = "Lnet/minecraft/world/chunk/Chunk;generateSkylightMap()V";

    // provideChunk

    @Inject(method = "provideChunk", at = @At("HEAD"))
    private void phiwg$provideHead(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.begin(GenProfile.PROVIDE);
    }

    @Inject(method = "provideChunk", at = @At(value = "INVOKE", target = CARVE, ordinal = 0))
    private void phiwg$cavesBegin(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.begin(GenProfile.CAVES);
    }

    @Inject(method = "provideChunk", at = @At(value = "INVOKE", target = CARVE, ordinal = 0, shift = At.Shift.AFTER))
    private void phiwg$cavesEnd(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.CAVES);
    }

    @Inject(method = "provideChunk", at = @At(value = "INVOKE", target = CARVE, ordinal = 1))
    private void phiwg$ravinesBegin(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.begin(GenProfile.RAVINES);
    }

    @Inject(method = "provideChunk", at = @At(value = "INVOKE", target = CARVE, ordinal = 1, shift = At.Shift.AFTER))
    private void phiwg$ravinesEnd(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.RAVINES);
        GenProfile.begin(GenProfile.STRUCTURES);
    }

    @Inject(method = "provideChunk", at = @At(value = "NEW", target = "net/minecraft/world/chunk/Chunk"))
    private void phiwg$chunkBegin(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.STRUCTURES);
        GenProfile.begin(GenProfile.CHUNK);
    }

    @Inject(method = "provideChunk", at = @At(value = "INVOKE", target = SKYLIGHT))
    private void phiwg$skylightBegin(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.CHUNK);
        GenProfile.begin(GenProfile.SKYLIGHT);
    }

    @Inject(method = "provideChunk", at = @At(value = "INVOKE", target = SKYLIGHT, shift = At.Shift.AFTER))
    private void phiwg$skylightEnd(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.SKYLIGHT);
    }

    @Inject(method = "provideChunk", at = @At("RETURN"))
    private void phiwg$provideReturn(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.PROVIDE);
    }

    // func_147424_a: the terrain shape

    @Inject(method = "func_147424_a", at = @At("HEAD"))
    private void phiwg$shapeHead(CallbackInfo ci) {
        GenProfile.begin(GenProfile.SHAPE);
    }

    @Inject(method = "func_147424_a", at = @At(value = "INVOKE", target = GEN_BIOMES))
    private void phiwg$biomes10Begin(CallbackInfo ci) {
        GenProfile.begin(GenProfile.BIOMES10);
    }

    @Inject(method = "func_147424_a", at = @At(value = "INVOKE", target = GEN_BIOMES, shift = At.Shift.AFTER))
    private void phiwg$biomes10End(CallbackInfo ci) {
        GenProfile.end(GenProfile.BIOMES10);
    }

    @Inject(method = "func_147424_a", at = @At(value = "INVOKE", target = NOISE, shift = At.Shift.AFTER))
    private void phiwg$interpBegin(CallbackInfo ci) {
        GenProfile.begin(GenProfile.INTERP);
    }

    @Inject(method = "func_147424_a", at = @At("RETURN"))
    private void phiwg$shapeReturn(CallbackInfo ci) {
        GenProfile.end(GenProfile.INTERP);
        GenProfile.end(GenProfile.SHAPE);
    }

    // func_147423_a: the noise field

    @Inject(method = "func_147423_a", at = @At("HEAD"))
    private void phiwg$noiseHead(CallbackInfo ci) {
        GenProfile.begin(GenProfile.NOISE);
        GenProfile.begin(GenProfile.OCTAVES);
    }

    @Inject(
        method = "func_147423_a",
        at = @At(
            value = "FIELD",
            target = "Lnet/minecraft/world/gen/ChunkProviderGenerate;field_147425_f:[D",
            opcode = Opcodes.PUTFIELD,
            shift = At.Shift.AFTER))
    private void phiwg$octavesDone(CallbackInfo ci) {
        GenProfile.end(GenProfile.OCTAVES);
    }

    @Inject(method = "func_147423_a", at = @At("RETURN"))
    private void phiwg$noiseReturn(CallbackInfo ci) {
        GenProfile.end(GenProfile.NOISE);
    }

    // replaceBlocksForBiome, populate

    @Inject(method = "provideChunk", at = @At(value = "INVOKE", target = SURFACE))
    private void phiwg$surfaceBegin(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.begin(GenProfile.SURFACE);
    }

    @Inject(method = "provideChunk", at = @At(value = "INVOKE", target = SURFACE, shift = At.Shift.AFTER))
    private void phiwg$surfaceEnd(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.SURFACE);
    }

    @Inject(method = "populate", at = @At("HEAD"))
    private void phiwg$populateHead(IChunkProvider provider, int x, int z, CallbackInfo ci) {
        GenProfile.begin(GenProfile.POPULATE);
    }

    @Inject(method = "populate", at = @At("RETURN"))
    private void phiwg$populateReturn(IChunkProvider provider, int x, int z, CallbackInfo ci) {
        GenProfile.end(GenProfile.POPULATE);
    }
}
