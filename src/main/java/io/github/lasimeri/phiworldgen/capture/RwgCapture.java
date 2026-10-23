package io.github.lasimeri.phiworldgen.capture;

import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import net.minecraft.world.WorldServer;
import net.minecraft.world.biome.BiomeGenBase;

/**
 * Captures of RWG (Realistic World Gen alpha 1.5.2, GT New Horizons' world type) for the card's replay: its
 * generator state once (rwg-world.bin), then per chunk what getNewNoise makes (h_X_Z.bin). RWG is read by
 * reflection, so the build needs no RWG jar. See RwgCapture.md and docs/rwg-capture-format.md.
 */
public final class RwgCapture {

    private RwgCapture() {}

    /** rNoise kinds (card/lib/rwg.h): a terrain class the biome delegates to, or the biome's own inline code. */
    private static final String[] TERRAINS = { "TerrainBase", "TerrainCanyon", "TerrainDunes", "TerrainDuneValley",
        "TerrainFlatLakes", "TerrainGrasslandFlats", "TerrainGrasslandHills", "TerrainGrasslandMountains",
        "TerrainHighland", "TerrainHilly", "TerrainMarsh", "TerrainMesa", "TerrainMountain", "TerrainMountainRiver",
        "TerrainMountainSpikes", "TerrainPolar", "TerrainSmallSupport", "TerrainSwampMountain", "TerrainSwampRiver" };
    /** The float fields each terrain's generateNoise reads, in the order the card expects them. */
    private static final Map<String, String[]> TERRAIN_FIELDS = new LinkedHashMap<>();
    static {
        TERRAIN_FIELDS.put("TerrainCanyon", new String[] { "strength", "base" }); // + smallRiver, height[6]
        TERRAIN_FIELDS.put("TerrainDuneValley", new String[] { "valley" });
        TERRAIN_FIELDS.put(
            "TerrainGrasslandHills",
            new String[] { "hHeight", "hWidth", "vHeight", "vWidth", "lHeight", "lWidth", "bHeight" });
        TERRAIN_FIELDS.put("TerrainHighland", new String[] { "start", "height", "base", "width" });
        TERRAIN_FIELDS
            .put("TerrainHilly", new String[] { "width", "strength", "lakeDepth", "lakeWidth", "terrainHeight" });
        TERRAIN_FIELDS.put("TerrainSwampMountain", new String[] { "heigth", "width" });
    }
    /** Biome classes with their own rNoise, numbered after the terrains (64 +). */
    private static final String[] INLINE = { "RealisticBiomeBase", "RealisticBiomeTest", "RealisticBiomeTestRiver",
        "RealisticBiomeOceanTest", "RealisticBiomeCoastColdCliff", "RealisticBiomeCoastColdSlope",
        "RealisticBiomeCoastDunes", "RealisticBiomeCoastIce", "RealisticBiomeCoastMangrove", "RealisticBiomeCoastOasis",
        "RealisticBiomeCoastTest", "RealisticBiomeIslandTropical", "RealisticBiomeIslandTundra" };
    /** Biome classes whose rNoise is exactly return this.terrain.generateNoise(...). */
    private static final String[] DELEGATING = { "RealisticBiomeCanyon", "RealisticBiomeCanyonForest",
        "RealisticBiomeDarkRedwood", "RealisticBiomeDarkRedwoodPlains", "RealisticBiomeDesert",
        "RealisticBiomeDesertMountains", "RealisticBiomeDuneValley", "RealisticBiomeDuneValleyForest",
        "RealisticBiomeHighRainforest", "RealisticBiomeHotForest", "RealisticBiomeHotRedwood",
        "RealisticBiomeJungleCanyon", "RealisticBiomeJungleHills", "RealisticBiomeMesa", "RealisticBiomeMesaPlains",
        "RealisticBiomeOasis", "RealisticBiomePolar", "RealisticBiomeRedDesertMountains", "RealisticBiomeRedOasis",
        "RealisticBiomeRedwood", "RealisticBiomeRedwoodJungle", "RealisticBiomeRedwoodSnow", "RealisticBiomeSavanna",
        "RealisticBiomeSavannaDunes", "RealisticBiomeSavannaForest", "RealisticBiomeSnowHills",
        "RealisticBiomeSnowLakes", "RealisticBiomeSnowRivers", "RealisticBiomeStoneMountains",
        "RealisticBiomeStoneMountainsCactus", "RealisticBiomeSupport", "RealisticBiomeTaigaHills",
        "RealisticBiomeTaigaPlains", "RealisticBiomeTundraHills", "RealisticBiomeTundraPlains",
        "RealisticBiomeWoodHills", "RealisticBiomeWoodMountains" };
    private static final String[] LISTS = { "biomes_snow", "biomes_cold", "biomes_hot", "biomes_wet", "biomes_small",
        "biomes_test" };

    /** Whether the overworld runs RWG with what the card can reproduce; the reason when not. */
    public static String refusal(WorldServer world) {
        Object gen = world.theChunkProviderServer.currentChunkProvider;
        if (!gen.getClass()
            .getName()
            .equals("rwg.world.ChunkGeneratorRealistic")) return "the overworld generator is not RWG's";
        try {
            Object perlin = get(gen, "perlin");
            if (!perlin.getClass()
                .getName()
                .equals("rwg.util.NoiseGeneratorWrapper"))
                return "RWG's noise is " + perlin.getClass()
                    .getName();
            if (Class.forName("rwg.util.NoiseGeneratorWrapper")
                .getField("useOpenSimplex")
                .getBoolean(null)) return "RWG uses OpenSimplex noise";
            for (Object b : biomeList()) if (b != null && kind(b) < 0) return "RWG biome class " + b.getClass()
                .getName() + " is not ported";
        } catch (ReflectiveOperationException | RuntimeException e) {
            return "RWG unreadable: " + e;
        }
        return null;
    }

    /** The generator state the card needs (docs/rwg-capture-format.md, rwg-world.bin). */
    public static byte[] worldBlob(WorldServer world) throws ReflectiveOperationException {
        Object gen = world.theChunkProviderServer.currentChunkProvider;
        Object cmr = world.getWorldChunkManager();
        ByteBuffer b = ByteBuffer.allocate(1 << 20)
            .order(ByteOrder.LITTLE_ENDIAN);
        b.put("RWGW".getBytes(StandardCharsets.US_ASCII))
            .putInt(1);
        int s = open(b, "SEED");
        b.putLong(world.getSeed());
        close(b, s);
        // the generator's and the biome manager's Perlin are both NoiseSelector's for the world seed: one table set
        Object perlin = get(get(gen, "perlin"), "innerPerlin");
        s = open(b, "NOIS");
        for (int v : (int[]) get(perlin, "p")) b.putInt(v);
        for (float v : (float[]) get(perlin, "g1")) b.putFloat(v);
        for (float v : (float[]) get(perlin, "g2")) b.putFloat(v);
        for (float v : (float[]) get(perlin, "g3")) b.putFloat(v);
        close(b, s);
        s = open(b, "CELL");
        for (Object c : new Object[] { get(gen, "cell"), get(cmr, "cell"), get(cmr, "biomecell") }) {
            b.putLong((Long) get(c, "seed"))
                .putLong((Long) get(c, "seedOffset"))
                .put((byte) ((Boolean) get(c, "useDistance") ? 1 : 0))
                .putShort((Short) get(c, "distanceMethod"));
        }
        close(b, s);
        s = open(b, "PARB");
        for (float v : (float[]) get(gen, "parabolicField")) b.putFloat(v);
        close(b, s);
        s = open(b, "LIST");
        b.put((byte) ((Boolean) get(cmr, "wetEnabled") ? 1 : 0))
            .put((byte) ((Boolean) get(cmr, "smallEnabled") ? 1 : 0));
        for (String name : LISTS) {
            List<?> l = (List<?>) get(cmr, name);
            b.putInt(l.size());
            for (Object o : l) b.putShort((short) (int) (Integer) get(o, "biomeID"));
        }
        close(b, s);
        s = open(b, "RBIO");
        Object[] all = biomeList();
        int n = 0;
        for (int i = 0; i < all.length; i++) if (all[i] != null) n = i + 1;
        b.putInt(n);
        for (int i = 0; i < n; i++) {
            Object o = all[i];
            b.put((byte) (o != null ? 1 : 0));
            if (o == null) continue;
            int k = kind(o);
            b.put((byte) k)
                .putShort((short) ((BiomeGenBase) get(o, "baseBiome")).biomeID)
                .putShort((short) ((BiomeGenBase) get(o, "riverBiome")).biomeID);
            float[] p = k < 64 ? terrainParams(get(o, "terrain")) : new float[0];
            b.put((byte) p.length);
            for (float v : p) b.putFloat(v);
        }
        close(b, s);
        return java.util.Arrays.copyOf(b.array(), b.position());
    }

    /**
     * Captures name: rwg-world.bin, then h_X_Z.bin for the chunk set (around the origin, far out, and the first
     * chunk found for each realistic biome within radius chunks). The generator's own getNewNoise makes them; its
     * mapGenBiomes entries are cleared after each call, as provideChunk does, so later chunks are unaffected.
     */
    public static String capture(WorldServer world, Path dir, int radius)
        throws IOException, ReflectiveOperationException {
        String refused = refusal(world);
        if (refused != null) throw new IllegalStateException(refused);
        Files.createDirectories(dir);
        Files.write(dir.resolve("rwg-world.bin"), worldBlob(world));
        Object gen = world.theChunkProviderServer.currentChunkProvider;
        Object cmr = world.getWorldChunkManager();
        Class<?> rbb = Class.forName("rwg.biomes.realistic.RealisticBiomeBase");
        Method newNoise = gen.getClass()
            .getMethod(
                "getNewNoise",
                cmr.getClass(),
                int.class,
                int.class,
                java.lang.reflect.Array.newInstance(rbb, 0)
                    .getClass());
        Method biomeAt = cmr.getClass()
            .getMethod("getBiomeDataAt", int.class, int.class);
        Field river = field(gen.getClass(), "riverStrength"), mapGen = field(gen.getClass(), "mapGenBiomes");

        Map<Long, int[]> set = new LinkedHashMap<>();
        for (int x = -3; x <= 2; x++) for (int z = -3; z <= 2; z++) add(set, x, z);
        // float coordinates lose their low bits past 2^24 blocks; the noise's x + 4096 then (int)
        int[][] far = { { 6200, 0 }, { -6200, 0 }, { 0, -6200 }, { 65536, 65536 }, { -500000, 500000 },
            { 1000000, -1000000 }, { -1800000, -1800000 } };
        for (int[] c : far) add(set, c[0], c[1]);
        Map<Integer, int[]> byBiome = new LinkedHashMap<>();
        for (int x = -radius; x <= radius; x += 4) for (int z = -radius; z <= radius; z += 4) {
            int id = (Integer) get(biomeAt.invoke(cmr, x * 16 + 8, z * 16 + 8), "biomeID");
            if (!byBiome.containsKey(id)) byBiome.put(id, new int[] { x, z });
        }
        for (int[] c : byBiome.values()) add(set, c[0], c[1]);

        Object biomes = java.lang.reflect.Array.newInstance(rbb, 256);
        ByteBuffer b = ByteBuffer.allocate(16 << 10)
            .order(ByteOrder.LITTLE_ENDIAN);
        for (int[] c : set.values()) {
            java.util.Arrays.fill((Object[]) biomes, null);
            float[] heights = (float[]) newNoise.invoke(gen, cmr, c[0] * 16, c[1] * 16, biomes);
            float[] mg = (float[]) mapGen.get(gen);
            b.clear();
            b.put("PWGH".getBytes(StandardCharsets.US_ASCII))
                .putInt(1);
            int s = open(b, "CHNK");
            b.putInt(c[0])
                .putInt(c[1]);
            close(b, s);
            s = open(b, "HGTS");
            for (float v : heights) b.putFloat(v);
            close(b, s);
            s = open(b, "RBIO");
            for (Object o : (Object[]) biomes) b.putShort((short) (o == null ? -1 : (Integer) get(o, "biomeID")));
            close(b, s);
            s = open(b, "RIVR");
            for (float v : (float[]) river.get(gen)) b.putFloat(v);
            close(b, s);
            s = open(b, "MGBI");
            for (float v : mg) b.putFloat(v);
            close(b, s);
            java.util.Arrays.fill(mg, 0, 256, 0.0F); // as provideChunk leaves them
            Files.write(
                dir.resolve(String.format("h_%d_%d.bin", c[0], c[1])),
                java.util.Arrays.copyOf(b.array(), b.position()));
        }
        return String.format(
            "%d chunks (%d realistic biomes found within %d chunks: %s)",
            set.size(),
            byBiome.size(),
            radius,
            byBiome.keySet());
    }

    /** The rNoise kind of a realistic biome: its terrain's index, 64 + its inline code's, or -1 (not ported). */
    private static int kind(Object biome) throws ReflectiveOperationException {
        String c = biome.getClass()
            .getSimpleName();
        for (int i = 0; i < INLINE.length; i++) if (INLINE[i].equals(c)) return 64 + i;
        for (String d : DELEGATING) if (d.equals(c)) {
            String t = get(biome, "terrain").getClass()
                .getSimpleName();
            for (int i = 0; i < TERRAINS.length; i++) if (TERRAINS[i].equals(t)) return i;
            return -1;
        }
        return -1;
    }

    private static float[] terrainParams(Object terrain) throws ReflectiveOperationException {
        String t = terrain.getClass()
            .getSimpleName();
        List<Float> p = new ArrayList<>();
        String[] names = TERRAIN_FIELDS.get(t);
        if (names != null) for (String n : names) p.add((Float) get(terrain, n));
        if (t.equals("TerrainCanyon")) {
            p.add((Boolean) get(terrain, "smallRiver") ? 1.0F : 0.0F);
            float[] h = (float[]) get(terrain, "height");
            p.add((float) h.length);
            for (float v : h) p.add(v);
        }
        float[] out = new float[p.size()];
        for (int i = 0; i < out.length; i++) out[i] = p.get(i);
        return out;
    }

    private static Object[] biomeList() throws ReflectiveOperationException {
        return (Object[]) field(Class.forName("rwg.biomes.realistic.RealisticBiomeBase"), "biomeList").get(null);
    }

    static Object get(Object o, String name) throws ReflectiveOperationException {
        return field(o.getClass(), name).get(o);
    }

    private static Field field(Class<?> c, String name) throws NoSuchFieldException {
        for (Class<?> k = c; k != null; k = k.getSuperclass()) {
            try {
                Field f = k.getDeclaredField(name);
                f.setAccessible(true);
                return f;
            } catch (NoSuchFieldException e) {
                // declared further up
            }
        }
        throw new NoSuchFieldException(c.getName() + "." + name);
    }

    private static void add(Map<Long, int[]> set, int x, int z) {
        set.put(((long) x << 32) ^ (z & 0xFFFFFFFFL), new int[] { x, z });
    }

    private static int open(ByteBuffer b, String tag) {
        b.put(tag.getBytes(StandardCharsets.US_ASCII))
            .putInt(0);
        return b.position();
    }

    private static void close(ByteBuffer b, int start) {
        b.putInt(start - 4, b.position() - start);
    }
}
