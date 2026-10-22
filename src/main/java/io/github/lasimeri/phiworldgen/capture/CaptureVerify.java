package io.github.lasimeri.phiworldgen.capture;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.nio.file.DirectoryStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.Map;

import net.minecraft.world.World;
import net.minecraft.world.biome.BiomeGenBase;
import net.minecraft.world.gen.ChunkProviderGenerate;
import net.minecraft.world.gen.NoiseGeneratorImproved;
import net.minecraft.world.gen.NoiseGeneratorOctaves;

import io.github.lasimeri.phiworldgen.mixins.AccessorChunkProviderGenerate;
import io.github.lasimeri.phiworldgen.mixins.AccessorNoiseGeneratorImproved;
import io.github.lasimeri.phiworldgen.mixins.AccessorNoiseGeneratorOctaves;

/** The S2 gate: the captured state alone reproduces the captured octaves and field. See CaptureVerify.md. */
public final class CaptureVerify {

    private CaptureVerify() {}

    public static String run(World world, Path dir) throws IOException {
        Map<String, ByteBuffer> w = sections(dir.resolve("world.bin"), "PWGW");
        ByteBuffer seed = w.get("SEED");
        long captured = seed.getLong();
        boolean features = seed.get() != 0;

        // Another seed, so that nothing but the captured state can make the outputs match.
        ChunkProviderGenerate p = new ChunkProviderGenerate(world, captured ^ 0x5DEECE66DL, features);
        AccessorChunkProviderGenerate a = (AccessorChunkProviderGenerate) p;
        ByteBuffer gens = w.get("GENS");
        int count = gens.getInt();
        NoiseGeneratorOctaves[] mine = Capture.generators(p);
        if (count != mine.length) return "verify: " + count + " generators captured, " + mine.length + " here";
        for (NoiseGeneratorOctaves g : mine) {
            NoiseGeneratorImproved[] octaves = ((AccessorNoiseGeneratorOctaves) g).phiwg$getGenerators();
            int n = gens.getInt();
            if (n != octaves.length) return "verify: octave count " + n + " captured, " + octaves.length + " here";
            for (NoiseGeneratorImproved o : octaves) {
                int[] perm = new int[512];
                for (int i = 0; i < 512; i++) perm[i] = gens.getInt();
                ((AccessorNoiseGeneratorImproved) o).phiwg$setPermutations(perm);
                o.xCoord = gens.getDouble();
                o.yCoord = gens.getDouble();
                o.zCoord = gens.getDouble();
            }
        }
        ByteBuffer parb = w.get("PARB");
        float[] parabolic = a.phiwg$getParabolicField();
        for (int i = 0; i < parabolic.length; i++) parabolic[i] = parb.getFloat();

        int chunks = 0, bad = 0;
        String first = "";
        try (DirectoryStream<Path> files = Files.newDirectoryStream(dir, "c_*.bin")) {
            for (Path f : files) {
                Map<String, ByteBuffer> c = sections(f, "PWGC");
                ByteBuffer pos = c.get("CHNK");
                int x = pos.getInt(), z = pos.getInt();
                ByteBuffer bi = c.get("BI10");
                BiomeGenBase[] biomes = new BiomeGenBase[100];
                for (int i = 0; i < 100; i++) biomes[i] = BiomeGenBase.getBiome(bi.getShort() & 0xFFFF);
                a.phiwg$setBiomes(biomes);
                a.phiwg$noiseField(x * 4, 0, z * 4);
                String d = diff("OCT0", c, a.phiwg$getDepthNoise(), 25);
                if (d == null) d = diff("OCT1", c, a.phiwg$getMainNoise(), 825);
                if (d == null) d = diff("OCT2", c, a.phiwg$getMinLimitNoise(), 825);
                if (d == null) d = diff("OCT3", c, a.phiwg$getMaxLimitNoise(), 825);
                if (d == null) d = diff("FLD ", c, a.phiwg$getField(), 825);
                chunks++;
                if (d != null) {
                    if (bad++ == 0) first = " first: chunk " + x + "," + z + " " + d;
                }
            }
        }
        return String.format(
            "phiwg verify %s: %d chunks, %d differ in the octaves or the field, from captured state on seed %d ^ 0x5DEECE66D%s",
            dir.getFileName(),
            chunks,
            bad,
            captured,
            first);
    }

    /** null when the captured doubles and these are the same bits, else the first index and both values. */
    private static String diff(String tag, Map<String, ByteBuffer> c, double[] mine, int n) {
        ByteBuffer b = c.get(tag);
        for (int i = 0; i < n; i++) {
            long want = b.getLong(), got = Double.doubleToRawLongBits(mine[i]);
            if (want != got) return String.format(
                "%s[%d] captured %s (%016x), recomputed %s (%016x)",
                tag.trim(),
                i,
                Double.longBitsToDouble(want),
                want,
                mine[i],
                got);
        }
        return null;
    }

    /** The sections of a capture file by tag, each buffer positioned at its payload. */
    static Map<String, ByteBuffer> sections(Path f, String magic) throws IOException {
        ByteBuffer b = ByteBuffer.wrap(Files.readAllBytes(f))
            .order(ByteOrder.LITTLE_ENDIAN);
        byte[] m = new byte[4];
        b.get(m);
        if (!new String(m, StandardCharsets.US_ASCII).equals(magic) || b.getInt() != 1)
            throw new IOException(f + ": not a version 1 " + magic + " file");
        Map<String, ByteBuffer> out = new HashMap<>();
        while (b.remaining() >= 8) {
            byte[] t = new byte[4];
            b.get(t);
            int len = b.getInt();
            ByteBuffer s = b.slice()
                .order(ByteOrder.LITTLE_ENDIAN);
            s.limit(len);
            out.put(new String(t, StandardCharsets.US_ASCII), s);
            b.position(b.position() + len);
        }
        return out;
    }
}
