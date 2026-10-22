package io.github.lasimeri.phiworldgen.mixins;

import net.minecraft.block.Block;
import net.minecraft.world.World;
import net.minecraft.world.WorldServer;
import net.minecraft.world.biome.BiomeGenBase;
import net.minecraft.world.biome.WorldChunkManager;
import net.minecraft.world.gen.ChunkProviderGenerate;

import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.Unique;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.Redirect;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import io.github.lasimeri.phiworldgen.capture.Capture;
import io.github.lasimeri.phiworldgen.card.CardClient;
import io.github.lasimeri.phiworldgen.card.CardShape;
import io.github.lasimeri.phiworldgen.profile.GenProfile;

/**
 * The terrain shape and the 16 x 16 biomes from the card when its answer is in, vanilla's otherwise. See
 * MixinChunkProviderGenerate_Card.md.
 */
@Mixin(ChunkProviderGenerate.class)
public abstract class MixinChunkProviderGenerate_Card {

    private static final String LOAD_BIOMES = "Lnet/minecraft/world/biome/WorldChunkManager;"
        + "loadBlockGeneratorData([Lnet/minecraft/world/biome/BiomeGenBase;IIII)[Lnet/minecraft/world/biome/BiomeGenBase;";

    @Shadow
    private World worldObj;

    @Unique
    private byte[] phiwg$checkRle;

    /** The card's answer for the chunk being provided (phiwg$ax, phiwg$az), for its biomes after the shape. */
    @Unique
    private byte[] phiwg$answer;

    @Unique
    private int phiwg$ax, phiwg$az;

    @Inject(method = "func_147424_a", at = @At("HEAD"), cancellable = true)
    private void phiwg$cardShape(int x, int z, Block[] blocks, CallbackInfo ci) {
        phiwg$answer = null;
        if (!CardShape.enabled || Capture.on() || !(worldObj instanceof WorldServer)) return;
        CardClient c = CardClient.get();
        if (c == null || c.provider != (Object) this) return; // the card holds this generator's state only
        GenProfile.begin(GenProfile.CARD);
        byte[] rle = CardShape.fetch((WorldServer) worldObj, x, z);
        GenProfile.end(GenProfile.CARD);
        if (rle == null) return; // vanilla runs on: its biomes, then the shape, on the CPU
        phiwg$answer = rle;
        phiwg$ax = x;
        phiwg$az = z;
        if (CardShape.check) {
            phiwg$checkRle = rle; // vanilla runs on; compared at the return
            return;
        }
        CardShape.decode(rle, blocks);
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
