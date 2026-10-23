package io.github.lasimeri.phiworldgen.latemixins;

import net.minecraft.block.Block;
import net.minecraft.world.World;
import net.minecraft.world.WorldServer;

import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
import org.spongepowered.asm.mixin.Unique;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import io.github.lasimeri.phiworldgen.card.CardClient;
import io.github.lasimeri.phiworldgen.card.CardShape;
import io.github.lasimeri.phiworldgen.card.RwgCard;
import rwg.biomes.realistic.RealisticBiomeBase;
import rwg.world.ChunkGeneratorRealistic;
import rwg.world.ChunkManagerRealistic;

/**
 * RWG's height stage from the card: generateTerrain takes the card's answer for its chunk when there is one, and
 * runs on the CPU when there is none (never asked, late, the card down). Check mode runs both and compares. The
 * rest of provideChunk (map gens, surface, caves, structures) is RWG's own. See MixinChunkGeneratorRealistic_Card.md.
 */
@Mixin(value = ChunkGeneratorRealistic.class, remap = false)
public abstract class MixinChunkGeneratorRealistic_Card {

    @Shadow
    @Final
    private World worldObj;
    @Shadow
    @Final
    private float[] testHeight;
    @Shadow
    @Final
    private float[] riverStrength;
    @Shadow
    @Final
    private float[] mapGenBiomes;

    /** The answer check mode holds from the head of generateTerrain to its return. */
    @Unique
    private byte[] phiwg$checking;

    @Inject(method = "generateTerrain", at = @At("HEAD"), cancellable = true)
    private void phiwg$terrain(ChunkManagerRealistic cmr, int cx, int cz, Block[] blocks, byte[] metas,
        RealisticBiomeBase[] biomes, CallbackInfo ci) {
        phiwg$checking = null;
        CardClient c = CardClient.get();
        if (!CardShape.enabled || c == null || c.provider != (Object) this) return;
        byte[] a = CardShape.fetch((WorldServer) worldObj, cx, cz);
        if (!RwgCard.isHeights(a)) return;
        if (CardShape.check) {
            phiwg$checking = a;
            return;
        }
        if (RwgCard.apply(a, blocks, biomes, testHeight, riverStrength, mapGenBiomes)) {
            RwgCard.used++;
            ci.cancel();
        }
    }

    @Inject(method = "generateTerrain", at = @At("RETURN"))
    private void phiwg$terrainCheck(ChunkManagerRealistic cmr, int cx, int cz, Block[] blocks, byte[] metas,
        RealisticBiomeBase[] biomes, CallbackInfo ci) {
        byte[] a = phiwg$checking;
        if (a == null) return;
        phiwg$checking = null;
        RwgCard.compare(cx, cz, a, blocks, biomes, testHeight, riverStrength, mapGenBiomes);
    }
}
