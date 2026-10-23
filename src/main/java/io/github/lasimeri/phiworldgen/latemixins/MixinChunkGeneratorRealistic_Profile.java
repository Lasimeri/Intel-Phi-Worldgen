package io.github.lasimeri.phiworldgen.latemixins;

import net.minecraft.world.chunk.Chunk;
import net.minecraft.world.chunk.IChunkProvider;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Pseudo;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.github.lasimeri.phiworldgen.profile.GenProfile;

/** Marks the phases of RWG's overworld generation for GenProfile. See MixinChunkGeneratorRealistic_Profile.md. */
// RWG ships compiled to SRG names (as the game runs them): every target here is written in them, remap off; the
// class is named, not imported, so the build needs no RWG jar
@Pseudo
@Mixin(targets = "rwg.world.ChunkGeneratorRealistic", remap = false)
public abstract class MixinChunkGeneratorRealistic_Profile {

    private static final String TERRAIN = "Lrwg/world/ChunkGeneratorRealistic;generateTerrain(Lrwg/world/ChunkManagerRealistic;II[Lnet/minecraft/block/Block;[B[Lrwg/biomes/realistic/RealisticBiomeBase;)V";
    private static final String SURFACE = "Lrwg/world/ChunkGeneratorRealistic;replaceBlocksForBiome(II[Lnet/minecraft/block/Block;[B[Lrwg/biomes/realistic/RealisticBiomeBase;[Lnet/minecraft/world/biome/BiomeGenBase;[F)V";
    private static final String CARVE = "Lnet/minecraft/world/gen/MapGenBase;func_151539_a(Lnet/minecraft/world/chunk/IChunkProvider;Lnet/minecraft/world/World;II[Lnet/minecraft/block/Block;)V";
    private static final String SKYLIGHT = "Lnet/minecraft/world/chunk/Chunk;func_76603_b()V";

    @Inject(method = "func_73154_d", at = @At("HEAD"))
    private void phiwg$provideHead(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.begin(GenProfile.PROVIDE);
    }

    @Inject(method = "func_73154_d", at = @At(value = "INVOKE", target = TERRAIN))
    private void phiwg$terrainBegin(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.begin(GenProfile.RWG_TERRAIN);
    }

    // generateTerrain returns: the per-biome map gens run until replaceBlocksForBiome
    @Inject(method = "func_73154_d", at = @At(value = "INVOKE", target = TERRAIN, shift = At.Shift.AFTER))
    private void phiwg$terrainEnd(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.RWG_TERRAIN);
        GenProfile.begin(GenProfile.RWG_MAPGEN);
    }

    @Inject(method = "func_73154_d", at = @At(value = "INVOKE", target = SURFACE))
    private void phiwg$surfaceBegin(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.RWG_MAPGEN);
        GenProfile.begin(GenProfile.SURFACE);
    }

    // the caves follow (RWG config "Generate Caves"), then the structures
    @Inject(method = "func_73154_d", at = @At(value = "INVOKE", target = SURFACE, shift = At.Shift.AFTER))
    private void phiwg$surfaceEnd(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.SURFACE);
    }

    @Inject(method = "func_73154_d", at = @At(value = "INVOKE", target = CARVE, ordinal = 0))
    private void phiwg$cavesBegin(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.begin(GenProfile.CAVES);
    }

    @Inject(method = "func_73154_d", at = @At(value = "INVOKE", target = CARVE, ordinal = 0, shift = At.Shift.AFTER))
    private void phiwg$cavesEnd(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.CAVES);
        GenProfile.begin(GenProfile.STRUCTURES);
    }

    @Inject(method = "func_73154_d", at = @At(value = "NEW", target = "net/minecraft/world/chunk/Chunk"))
    private void phiwg$chunkBegin(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.STRUCTURES);
        GenProfile.begin(GenProfile.CHUNK);
    }

    @Inject(method = "func_73154_d", at = @At(value = "INVOKE", target = SKYLIGHT))
    private void phiwg$skylightBegin(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.CHUNK);
        GenProfile.begin(GenProfile.SKYLIGHT);
    }

    @Inject(method = "func_73154_d", at = @At(value = "INVOKE", target = SKYLIGHT, shift = At.Shift.AFTER))
    private void phiwg$skylightEnd(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.SKYLIGHT);
    }

    @Inject(method = "func_73154_d", at = @At("RETURN"))
    private void phiwg$provideReturn(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        GenProfile.end(GenProfile.PROVIDE);
    }

    @Inject(method = "getNewNoise", at = @At("HEAD"))
    private void phiwg$heightsHead(CallbackInfoReturnable<float[]> cir) {
        GenProfile.begin(GenProfile.RWG_HEIGHTS);
    }

    @Inject(method = "getNewNoise", at = @At("RETURN"))
    private void phiwg$heightsReturn(CallbackInfoReturnable<float[]> cir) {
        GenProfile.end(GenProfile.RWG_HEIGHTS);
    }

    @Inject(method = "func_73153_a", at = @At("HEAD"))
    private void phiwg$populateHead(IChunkProvider provider, int x, int z, CallbackInfo ci) {
        GenProfile.begin(GenProfile.POPULATE);
    }

    @Inject(method = "func_73153_a", at = @At("RETURN"))
    private void phiwg$populateReturn(IChunkProvider provider, int x, int z, CallbackInfo ci) {
        GenProfile.end(GenProfile.POPULATE);
    }
}
