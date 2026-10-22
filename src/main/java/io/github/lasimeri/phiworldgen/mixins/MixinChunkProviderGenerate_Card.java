package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.block.Block;
import net.minecraft.world.World;
import net.minecraft.world.WorldServer;
import net.minecraft.world.biome.BiomeGenBase;
import net.minecraft.world.biome.WorldChunkManager;
import net.minecraft.world.chunk.Chunk;
import net.minecraft.world.chunk.IChunkProvider;
import net.minecraft.world.gen.ChunkProviderGenerate;
import net.minecraft.world.gen.MapGenBase;

import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.Unique;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.Redirect;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

import io.github.lasimeri.phiworldgen.capture.Capture;
import io.github.lasimeri.phiworldgen.card.CardClient;
import io.github.lasimeri.phiworldgen.card.CardShape;
import io.github.lasimeri.phiworldgen.profile.GenProfile;

/**
 * The card's chunk when its answer is in: the whole chunk (biomes, shape, surface, caves, ravines), or the shape
 * and the 16 x 16 biomes; vanilla's otherwise. See MixinChunkProviderGenerate_Card.md.
 */
@Mixin(ChunkProviderGenerate.class)
public abstract class MixinChunkProviderGenerate_Card {

    private static final String LOAD_BIOMES = "Lnet/minecraft/world/biome/WorldChunkManager;"
        + "loadBlockGeneratorData([Lnet/minecraft/world/biome/BiomeGenBase;IIII)[Lnet/minecraft/world/biome/BiomeGenBase;";

    @Shadow
    private World worldObj;

    @Shadow
    private BiomeGenBase[] biomesForGeneration;

    @Shadow
    @Final
    private boolean mapFeaturesEnabled;

    @Shadow
    private net.minecraft.world.gen.structure.MapGenStronghold strongholdGenerator;

    @Shadow
    private net.minecraft.world.gen.structure.MapGenVillage villageGenerator;

    @Shadow
    private net.minecraft.world.gen.structure.MapGenMineshaft mineshaftGenerator;

    @Shadow
    private net.minecraft.world.gen.structure.MapGenScatteredFeature scatteredFeatureGenerator;

    @Unique
    private byte[] phiwg$checkRle;

    /** The card's answer for the chunk being provided (phiwg$ax, phiwg$az): its shape and biomes, later in it. */
    @Unique
    private byte[] phiwg$answer;

    /** Check mode: a whole answer, compared with the chunk vanilla returns. */
    @Unique
    private byte[] phiwg$wholeCheck;

    @Unique
    private int phiwg$ax, phiwg$az;

    @Inject(method = "provideChunk", at = @At("HEAD"), cancellable = true)
    private void phiwg$cardChunk(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        phiwg$answer = null;
        phiwg$wholeCheck = null;
        if (!CardShape.enabled || Capture.on() || !(worldObj instanceof WorldServer)) return;
        CardClient c = CardClient.get();
        if (c == null || c.provider != (Object) this) return; // the card holds this generator's state only
        GenProfile.begin(GenProfile.CARD);
        byte[] a = CardShape.fetch((WorldServer) worldObj, x, z);
        GenProfile.end(GenProfile.CARD);
        if (a == null) return; // vanilla runs on: its biomes, then the shape, on the CPU
        phiwg$answer = a;
        phiwg$ax = x;
        phiwg$az = z;
        if (!CardShape.isWhole(a)) return; // the shape and biomes are taken further in
        if (CardShape.check) {
            phiwg$wholeCheck = a; // vanilla makes the chunk; compared at the return
            return;
        }
        // the whole chunk from the card; what stays here is vanilla's tail of provideChunk
        if (!GenProfile.isOpen(GenProfile.PROVIDE)) GenProfile.begin(GenProfile.PROVIDE);
        Block[] blocks = new Block[65536];
        byte[] metas = new byte[65536];
        CardShape.decodeWhole(a, blocks, metas);
        biomesForGeneration = CardShape.biomes(a, biomesForGeneration);
        CardShape.applyStates(a);
        phiwg$answer = null;
        IChunkProvider self = (IChunkProvider) (Object) this;
        GenProfile.begin(GenProfile.STRUCTURES);
        if (mapFeaturesEnabled) {
            ((MapGenBase) mineshaftGenerator).func_151539_a(self, worldObj, x, z, blocks);
            ((MapGenBase) villageGenerator).func_151539_a(self, worldObj, x, z, blocks);
            ((MapGenBase) strongholdGenerator).func_151539_a(self, worldObj, x, z, blocks);
            ((MapGenBase) scatteredFeatureGenerator).func_151539_a(self, worldObj, x, z, blocks);
        }
        GenProfile.end(GenProfile.STRUCTURES);
        GenProfile.begin(GenProfile.CHUNK);
        Chunk chunk = new Chunk(worldObj, blocks, metas, x, z);
        byte[] ids = chunk.getBiomeArray();
        for (int k = 0; k < ids.length; ++k) ids[k] = (byte) biomesForGeneration[k].biomeID;
        GenProfile.end(GenProfile.CHUNK);
        GenProfile.begin(GenProfile.SKYLIGHT);
        chunk.generateSkylightMap();
        GenProfile.end(GenProfile.SKYLIGHT);
        CardShape.wholeUsed++;
        GenProfile.end(GenProfile.PROVIDE); // the profiler's RETURN injection does not run on a cancel
        cir.setReturnValue(chunk);
    }

    @Inject(method = "provideChunk", at = @At("RETURN"))
    private void phiwg$cardWholeCheck(int x, int z, CallbackInfoReturnable<Chunk> cir) {
        byte[] a = phiwg$wholeCheck;
        if (a == null) return;
        phiwg$wholeCheck = null;
        CardShape.compareWhole(x, z, a, cir.getReturnValue());
    }

    @Inject(method = "func_147424_a", at = @At("HEAD"), cancellable = true)
    private void phiwg$cardShape(int x, int z, Block[] blocks, CallbackInfo ci) {
        byte[] a = phiwg$answer;
        if (a == null || x != phiwg$ax || z != phiwg$az || CardShape.isWhole(a)) return;
        if (CardShape.check) {
            phiwg$checkRle = a; // vanilla runs on; compared at the return
            return;
        }
        CardShape.decode(a, blocks);
        GenProfile.endIfOpen(GenProfile.SHAPE);
        ci.cancel();
    }

    @Inject(method = "func_147424_a", at = @At("RETURN"))
    private void phiwg$cardCheck(int x, int z, Block[] blocks, CallbackInfo ci) {
        byte[] rle = phiwg$checkRle;
        if (rle == null) return;
        phiwg$checkRle = null;
        CardShape.compare(x, z, rle, blocks);
    }

    /** provideChunk's 16 x 16 biomes: the card's when its answer for this chunk carries them (timed as BIOMES16). */
    @Redirect(method = "provideChunk", at = @At(value = "INVOKE", target = LOAD_BIOMES))
    private BiomeGenBase[] phiwg$biomes16(WorldChunkManager m, BiomeGenBase[] list, int x, int z, int w, int h) {
        GenProfile.begin(GenProfile.BIOMES16);
        byte[] a = phiwg$answer;
        phiwg$answer = null;
        BiomeGenBase[] out;
        if (a != null && CardShape.hasBiomes(a) && w == 16 && h == 16 && x == phiwg$ax * 16 && z == phiwg$az * 16) {
            if (CardShape.check) {
                out = m.loadBlockGeneratorData(list, x, z, w, h);
                CardShape.compareBiomes(phiwg$ax, phiwg$az, a, out);
            } else {
                out = CardShape.biomes(a, list);
                CardShape.biomesUsed++;
            }
        } else {
            out = m.loadBlockGeneratorData(list, x, z, w, h);
        }
        GenProfile.end(GenProfile.BIOMES16);
        return out;
    }
}
