package io.github.lasimeri.phiworldgen.card;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import net.minecraft.block.Block;
import net.minecraft.init.Blocks;

import io.github.lasimeri.phiworldgen.PhiWorldgen;
import rwg.biomes.realistic.RealisticBiomeBase;

/**
 * The card's answer for an RWG world (pwgd AN_RWGH): RWG's height stage, which the generator then fills, paints and
 * carves on the CPU as it would its own. Loaded only with RWG present (latemixins). See RwgCard.md.
 */
public final class RwgCard {

    /** u32 flags, f32 height[256], i16 biome[256], f32 river[256], f32 mapgen[258] */
    private static final int AN_RWGH = 4, BYTES = 4 + 1024 + 512 + 1024 + 1032;

    public static long used, checked, mismatched;

    private RwgCard() {}

    public static boolean isHeights(byte[] a) {
        return a != null && a.length >= BYTES
            && (ByteBuffer.wrap(a)
                .order(ByteOrder.LITTLE_ENDIAN)
                .getInt(0) & AN_RWGH) != 0;
    }

    /**
     * What generateTerrain leaves, from the answer: the generator's testHeight, riverStrength and mapGenBiomes, the
     * column biomes, and the blocks (stone up to the height, water below 63, air). False (nothing written) when a
     * column has no biome, where RWG itself would fail later.
     */
    public static boolean apply(byte[] a, Block[] blocks, RealisticBiomeBase[] biomes, float[] height, float[] river,
        float[] mapGen) {
        ByteBuffer b = ByteBuffer.wrap(a)
            .order(ByteOrder.LITTLE_ENDIAN);
        for (int i = 0; i < 256; i++) if (b.getShort(4 + 1024 + 2 * i) < 0) return false;
        for (int i = 0; i < 256; i++) {
            height[i] = b.getFloat(4 + 4 * i);
            biomes[i] = RealisticBiomeBase.getBiome(b.getShort(4 + 1024 + 2 * i));
            river[i] = b.getFloat(4 + 1536 + 4 * i);
        }
        for (int i = 0; i < 258; i++) mapGen[i] = b.getFloat(4 + 2560 + 4 * i);
        fill(height, blocks);
        return true;
    }

    /** generateTerrain's loop over getNewNoise's heights. */
    static void fill(float[] noise, Block[] blocks) {
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 16; j++) {
                int h = (int) noise[j * 16 + i];
                for (int k = 0; k < 256; k++) {
                    int p = (j * 16 + i) * 256 + k;
                    blocks[p] = k > h ? (k < 63 ? Blocks.water : Blocks.air) : Blocks.stone;
                }
            }
        }
    }

    /** Check mode: the answer against what generateTerrain just made on the CPU, bit for bit. */
    public static void compare(int x, int z, byte[] a, Block[] blocks, RealisticBiomeBase[] biomes, float[] height,
        float[] river, float[] mapGen) {
        checked++;
        Block[] cb = new Block[blocks.length];
        RealisticBiomeBase[] cbi = new RealisticBiomeBase[256];
        float[] ch = new float[256], cr = new float[256], cm = new float[258];
        String why = null;
        if (!apply(a, cb, cbi, ch, cr, cm)) why = "a column without a biome";
        else {
            int nb = 0, nbi = 0, nh = 0, nr = 0, nm = 0;
            for (int i = 0; i < blocks.length; i++) if (cb[i] != blocks[i]) nb++;
            for (int i = 0; i < 256; i++) {
                if (cbi[i] != biomes[i]) nbi++;
                if (Float.floatToRawIntBits(ch[i]) != Float.floatToRawIntBits(height[i])) nh++;
                if (Float.floatToRawIntBits(cr[i]) != Float.floatToRawIntBits(river[i])) nr++;
            }
            for (int i = 0; i < 258; i++)
                if (Float.floatToRawIntBits(cm[i]) != Float.floatToRawIntBits(mapGen[i])) nm++;
            if (nb + nbi + nh + nr + nm > 0) why = String.format(
                "blocks %d, biomes %d, heights %d, rivers %d, mapGenBiomes %d differ (first height %s card %s)",
                nb,
                nbi,
                nh,
                nr,
                nm,
                height[0],
                ch[0]);
        }
        if (why != null && mismatched++ < 10) PhiWorldgen.LOG.warn("phiwg card check: RWG chunk {},{}: {}", x, z, why);
    }

    public static String status() {
        return String.format("RWG height stages used %d; check: %d chunks, %d differ", used, checked, mismatched);
    }

    public static void resetStats() {
        used = checked = mismatched = 0;
    }
}
