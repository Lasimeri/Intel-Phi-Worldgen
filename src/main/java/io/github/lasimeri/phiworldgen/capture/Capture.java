package io.github.lasimeri.phiworldgen.capture;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;

import net.minecraft.block.Block;
import net.minecraft.world.World;
import net.minecraft.block.material.Material;
import net.minecraft.init.Blocks;
import net.minecraft.world.biome.BiomeGenBase;
import net.minecraft.world.biome.BiomeGenBeach;
import net.minecraft.world.biome.BiomeGenDesert;
import net.minecraft.world.biome.BiomeGenEnd;
import net.minecraft.world.biome.BiomeGenForest;
import net.minecraft.world.biome.BiomeGenHell;
import net.minecraft.world.biome.BiomeGenHills;
import net.minecraft.world.biome.BiomeGenJungle;
import net.minecraft.world.biome.BiomeGenMesa;
import net.minecraft.world.biome.BiomeGenMushroomIsland;
import net.minecraft.world.biome.BiomeGenMutated;
import net.minecraft.world.biome.BiomeGenOcean;
import net.minecraft.world.biome.BiomeGenPlains;
import net.minecraft.world.biome.BiomeGenRiver;
import net.minecraft.world.biome.BiomeGenSavanna;
import net.minecraft.world.biome.BiomeGenSnow;
import net.minecraft.world.biome.BiomeGenStoneBeach;
import net.minecraft.world.biome.BiomeGenSwamp;
import net.minecraft.world.biome.BiomeGenTaiga;
import net.minecraft.world.biome.WorldChunkManager;
import net.minecraft.world.gen.ChunkProviderGenerate;
import net.minecraft.world.gen.layer.GenLayer;
import net.minecraft.world.gen.layer.GenLayerAddIsland;
import net.minecraft.world.gen.layer.GenLayerAddMushroomIsland;
import net.minecraft.world.gen.layer.GenLayerAddSnow;
import net.minecraft.world.gen.layer.GenLayerBiome;
import net.minecraft.world.gen.layer.GenLayerBiomeEdge;
import net.minecraft.world.gen.layer.GenLayerDeepOcean;
import net.minecraft.world.gen.layer.GenLayerEdge;
import net.minecraft.world.gen.layer.GenLayerFuzzyZoom;
import net.minecraft.world.gen.layer.GenLayerHills;
import net.minecraft.world.gen.layer.GenLayerIsland;
import net.minecraft.world.gen.layer.GenLayerRareBiome;
import net.minecraft.world.gen.layer.GenLayerRemoveTooMuchOcean;
import net.minecraft.world.gen.layer.GenLayerRiver;
import net.minecraft.world.gen.layer.GenLayerRiverInit;
import net.minecraft.world.gen.layer.GenLayerRiverMix;
import net.minecraft.world.gen.layer.GenLayerShore;
import net.minecraft.world.gen.layer.GenLayerSmooth;
import net.minecraft.world.gen.layer.GenLayerVoronoiZoom;
import net.minecraft.world.gen.layer.GenLayerZoom;
import net.minecraftforge.common.BiomeManager;
import net.minecraft.world.gen.MapGenCaves;
import net.minecraft.world.gen.MapGenRavine;
import net.minecraft.world.gen.NoiseGeneratorImproved;
import net.minecraft.world.gen.NoiseGeneratorOctaves;
import net.minecraft.world.gen.NoiseGeneratorPerlin;
import net.minecraft.world.gen.NoiseGeneratorSimplex;

import io.github.lasimeri.phiworldgen.mixins.AccessorBiomeGenHills;
import io.github.lasimeri.phiworldgen.mixins.AccessorBiomeGenMutated;
import io.github.lasimeri.phiworldgen.mixins.AccessorBiomeGenTaiga;
import io.github.lasimeri.phiworldgen.mixins.AccessorChunkProviderGenerate;
import io.github.lasimeri.phiworldgen.mixins.AccessorNoiseGeneratorPerlin;
import io.github.lasimeri.phiworldgen.mixins.AccessorNoiseGeneratorSimplex;
import io.github.lasimeri.phiworldgen.mixins.AccessorGenLayer;
import io.github.lasimeri.phiworldgen.mixins.AccessorGenLayerBiome;
import io.github.lasimeri.phiworldgen.mixins.AccessorGenLayerEdge;
import io.github.lasimeri.phiworldgen.mixins.AccessorGenLayerHills;
import io.github.lasimeri.phiworldgen.mixins.AccessorGenLayerRiverMix;
import io.github.lasimeri.phiworldgen.mixins.AccessorMapGenBase;
import io.github.lasimeri.phiworldgen.mixins.AccessorWorldChunkManager;
import io.github.lasimeri.phiworldgen.mixins.AccessorMathHelper;
import io.github.lasimeri.phiworldgen.mixins.AccessorNoiseGeneratorImproved;
import io.github.lasimeri.phiworldgen.mixins.AccessorNoiseGeneratorOctaves;

/** Writes the generator's state and each captured chunk's stage boundaries in docs/capture-format.md. */
public final class Capture {

    public static final int FLAG_SURFACE_DENIED = 1;

    private static Path dir;
    private static ByteBuffer chunk;
    private static int flags;
    private static short[] surface;
    private static long carvedCaves, carvedRavines;

    private Capture() {}

    public static boolean on() {
        return chunk != null;
    }

    /** The overworld generators a capture covers, in the order func_147423_a calls them. */
    public static NoiseGeneratorOctaves[] generators(ChunkProviderGenerate p) {
        AccessorChunkProviderGenerate a = (AccessorChunkProviderGenerate) p;
        return new NoiseGeneratorOctaves[] { p.noiseGen6, a.phiwg$getMain(), a.phiwg$getMinLimit(),
            a.phiwg$getMaxLimit() };
    }

    /** Refuses a generator that is not exactly vanilla's, then writes world.bin into d. */
    public static void start(Path d, World world, ChunkProviderGenerate p) throws IOException {
        byte[] blob = worldBlob(world, p);
        Files.createDirectories(d);
        Files.write(d.resolve("world.bin"), blob);
        dir = d;
        carvedCaves = carvedRavines = 0;
    }

    /** world.bin's bytes for this generator (also what the card daemon takes); refuses a non-vanilla one. */
    public static byte[] worldBlob(World world, ChunkProviderGenerate p) {
        AccessorChunkProviderGenerate a = (AccessorChunkProviderGenerate) p;
        if (p.getClass() != ChunkProviderGenerate.class) refuse("provider " + p.getClass());
        for (NoiseGeneratorOctaves g : generators(p))
            if (g.getClass() != NoiseGeneratorOctaves.class) refuse("noise generator " + g.getClass());
        if (a.phiwg$getSurfaceNoise()
            .getClass() != NoiseGeneratorPerlin.class)
            refuse(
                "surface noise " + a.phiwg$getSurfaceNoise()
                    .getClass());
        if (a.phiwg$getCaves()
            .getClass() != MapGenCaves.class)
            refuse(
                "caves " + a.phiwg$getCaves()
                    .getClass());
        if (a.phiwg$getRavines()
            .getClass() != MapGenRavine.class)
            refuse(
                "ravines " + a.phiwg$getRavines()
                    .getClass());

        ByteBuffer b = ByteBuffer.allocate(1 << 20)
            .order(ByteOrder.LITTLE_ENDIAN);
        b.put("PWGW".getBytes(StandardCharsets.US_ASCII))
            .putInt(1);

        int s = open(b, "SEED");
        byte[] type = a.phiwg$getWorldType()
            .getWorldTypeName()
            .getBytes(StandardCharsets.UTF_8);
        b.putLong(world.getSeed())
            .put((byte) (a.phiwg$getMapFeatures() ? 1 : 0))
            .putShort((short) type.length)
            .put(type);
        close(b, s);

        s = open(b, "GENS");
        NoiseGeneratorOctaves[] gens = generators(p);
        b.putInt(gens.length);
        for (NoiseGeneratorOctaves g : gens) {
            NoiseGeneratorImproved[] octaves = ((AccessorNoiseGeneratorOctaves) g).phiwg$getGenerators();
            b.putInt(octaves.length);
            for (NoiseGeneratorImproved o : octaves) {
                for (int v : ((AccessorNoiseGeneratorImproved) o).phiwg$getPermutations()) b.putInt(v);
                b.putDouble(o.xCoord)
                    .putDouble(o.yCoord)
                    .putDouble(o.zCoord);
            }
        }
        close(b, s);

        s = open(b, "PARB");
        for (float f : a.phiwg$getParabolicField()) b.putFloat(f);
        close(b, s);

        s = open(b, "BIOM");
        BiomeGenBase[] list = BiomeGenBase.getBiomeGenArray();
        b.putInt(list.length);
        for (BiomeGenBase g : list) {
            byte[] name = g == null ? new byte[0]
                : g.getClass()
                    .getName()
                    .getBytes(StandardCharsets.UTF_8);
            b.put((byte) (g == null ? 0 : 1))
                .putFloat(g == null ? 0 : g.rootHeight)
                .putFloat(g == null ? 0 : g.heightVariation)
                .putShort(g == null ? 0 : blockId(g.topBlock))
                .putShort(g == null ? 0 : blockId(g.fillerBlock))
                .putShort((short) name.length)
                .put(name);
        }
        close(b, s);

        s = open(b, "SINT");
        for (float f : AccessorMathHelper.phiwg$getSinTable()) b.putFloat(f);
        close(b, s);

        biomeTraits(b);
        layers(b, world);
        surfaceTables(b, p);
        return java.util.Arrays.copyOf(b.array(), b.position());
    }

    /** The surface kinds of card/lib/surface.h: which genTerrainBlocks a biome runs. */
    public static final int SK_DEFAULT = 0, SK_HILLS = 1, SK_SAVANNA_M = 2, SK_TAIGA = 3, SK_UNPORTED = 4,
        SK_MUTATED = 5;

    /** Vanilla classes whose genTerrainBlocks is genBiomeTerrain (BiomeGenSwamp's extra loop stops at y 255). */
    private static final Class<?>[] PLAIN_SURFACE = { BiomeGenBase.class, BiomeGenOcean.class, BiomeGenPlains.class,
        BiomeGenDesert.class, BiomeGenForest.class, BiomeGenRiver.class, BiomeGenHell.class, BiomeGenEnd.class,
        BiomeGenSnow.class, BiomeGenMushroomIsland.class, BiomeGenBeach.class, BiomeGenJungle.class,
        BiomeGenStoneBeach.class, BiomeGenSavanna.class, BiomeGenSwamp.class };

    public static int surfaceKind(BiomeGenBase g) {
        Class<?> c = g.getClass();
        if (c == BiomeGenSavanna.Mutated.class) return SK_SAVANNA_M;
        if (c == BiomeGenHills.class) return SK_HILLS;
        if (c == BiomeGenTaiga.class) return SK_TAIGA;
        // BiomeGenMutated itself, or vanilla's anonymous subclasses of it (BiomeGenForest's), which keep its surface
        if (g instanceof BiomeGenMutated && (c == BiomeGenMutated.class || (c.getSuperclass() == BiomeGenMutated.class
            && java.util.Arrays.asList(PLAIN_SURFACE)
                .contains(c.getEnclosingClass())))) return SK_MUTATED;
        return java.util.Arrays.asList(PLAIN_SURFACE)
            .contains(c) ? SK_DEFAULT : SK_UNPORTED;
    }

    private static int slotOf(BiomeGenBase[] list, BiomeGenBase g) {
        for (int i = 0; i < list.length; i++) if (list[i] == g) return i;
        return -1;
    }

    /**
     * SURB: each biome slot's surface (kind, mutated base, top meta, the Hills variant flags, the Taiga type, air
     * flags, the first slot holding the same object, top and filler ids, temperature); SNOI: the stone noise
     * (field_147430_m); BLKS: the ids of the blocks the surface and the carvers place.
     */
    private static void surfaceTables(ByteBuffer b, ChunkProviderGenerate p) {
        BiomeGenBase[] list = BiomeGenBase.getBiomeGenArray();
        int s = open(b, "SURB");
        b.putInt(list.length);
        for (int i = 0; i < list.length; i++) {
            BiomeGenBase g = list[i];
            if (g == null || g.topBlock == null || g.fillerBlock == null) {
                b.put(new byte[16]);
                continue;
            }
            int kind = surfaceKind(g), base = 0, hills = 0, taiga = 0;
            if (kind == SK_MUTATED) {
                base = slotOf(list, ((AccessorBiomeGenMutated) g).phiwg$getBase());
                if (base < 0) kind = SK_UNPORTED;
            }
            if (kind == SK_HILLS) {
                AccessorBiomeGenHills h = (AccessorBiomeGenHills) g;
                hills = (h.phiwg$getAH() == h.phiwg$getAG() ? 1 : 0) | (h.phiwg$getAH() != h.phiwg$getAF() ? 2 : 0);
            }
            if (kind == SK_TAIGA) taiga = ((AccessorBiomeGenTaiga) g).phiwg$getType();
            int air = (g.topBlock.getMaterial() == Material.air ? 1 : 0)
                | (g.fillerBlock.getMaterial() == Material.air ? 2 : 0);
            b.put((byte) 1)
                .put((byte) kind)
                .put((byte) Math.max(base, 0))
                .put((byte) (g.field_150604_aj & 255))
                .put((byte) hills)
                .put((byte) taiga)
                .put((byte) air)
                .put((byte) slotOf(list, g))
                .putShort(blockId(g.topBlock))
                .putShort(blockId(g.fillerBlock))
                .putFloat(g.temperature);
        }
        close(b, s);

        s = open(b, "SNOI");
        NoiseGeneratorSimplex[] octaves = ((AccessorNoiseGeneratorPerlin) ((AccessorChunkProviderGenerate) p)
            .phiwg$getSurfaceNoise()).phiwg$getOctaves();
        b.putInt(octaves.length);
        for (NoiseGeneratorSimplex o : octaves) {
            for (int v : ((AccessorNoiseGeneratorSimplex) o).phiwg$getPermutations()) b.putInt(v);
            b.putDouble(o.field_151612_b)
                .putDouble(o.field_151613_c)
                .putDouble(o.field_151610_d);
        }
        close(b, s);

        s = open(b, "BLKS");
        for (Block k : new Block[] { Blocks.air, Blocks.stone, Blocks.bedrock, Blocks.gravel, Blocks.sand,
            Blocks.sandstone, Blocks.water, Blocks.ice, Blocks.grass, Blocks.dirt, Blocks.lava, Blocks.flowing_water,
            Blocks.flowing_lava }) b.putShort(blockId(k));
        close(b, s);

        // CARV: the carvers' ranges (MapGenBase.range, 8 in vanilla)
        AccessorChunkProviderGenerate a = (AccessorChunkProviderGenerate) p;
        s = open(b, "CARV");
        b.putInt(((AccessorMapGenBase) a.phiwg$getCaves()).phiwg$getRange())
            .putInt(((AccessorMapGenBase) a.phiwg$getRavines()).phiwg$getRange());
        close(b, s);
    }

    /** SSTA: the fields the surface overrides rewrite, of every biome object that has such an override. */
    public static void surfaceState() {
        BiomeGenBase[] list = BiomeGenBase.getBiomeGenArray();
        int s = open(chunk, "SSTA");
        int at = chunk.position();
        chunk.putInt(0);
        int n = 0;
        for (int i = 0; i < list.length; i++) {
            BiomeGenBase g = list[i];
            if (g == null) continue;
            int k = surfaceKind(g);
            if (k != SK_HILLS && k != SK_SAVANNA_M && k != SK_TAIGA) continue;
            chunk.putShort((short) i)
                .putShort(blockId(g.topBlock))
                .putShort(blockId(g.fillerBlock))
                .put((byte) (g.field_150604_aj & 255))
                .put((byte) 0);
            n++;
        }
        chunk.putInt(at, n);
        close(chunk, s);
    }

    /** Whether a world.bin blob holds the section with this tag (docs/capture-format.md: magic, version, sections). */
    public static boolean hasSection(byte[] blob, String tag) {
        ByteBuffer b = ByteBuffer.wrap(blob)
            .order(ByteOrder.LITTLE_ENDIAN);
        byte[] want = tag.getBytes(StandardCharsets.US_ASCII);
        for (int p = 8; p + 8 <= blob.length; p += 8 + b.getInt(p + 4)) {
            if (blob[p] == want[0] && blob[p + 1] == want[1] && blob[p + 2] == want[2] && blob[p + 3] == want[3])
                return true;
        }
        return false;
    }

    /** The GenLayer classes the card ports, in the order of its layer kinds (card/lib/genlayer.h). */
    private static final Class<?>[] LAYER_KINDS = { GenLayerIsland.class, GenLayerFuzzyZoom.class, GenLayerZoom.class,
        GenLayerAddIsland.class, GenLayerRemoveTooMuchOcean.class, GenLayerAddSnow.class, GenLayerEdge.class,
        GenLayerAddMushroomIsland.class, GenLayerDeepOcean.class, GenLayerRiverInit.class, GenLayerBiome.class,
        GenLayerBiomeEdge.class, GenLayerHills.class, GenLayerRiver.class, GenLayerSmooth.class,
        GenLayerRareBiome.class, GenLayerShore.class, GenLayerRiverMix.class, GenLayerVoronoiZoom.class };

    /**
     * BIOX: what the layers ask of a biome id: whether it exists, its getBiomeClass (as an index), its temperature
     * category, and four flags (snow, BiomeGenMesa, Forge's ocean list, the jungle class).
     */
    private static void biomeTraits(ByteBuffer b) {
        int s = open(b, "BIOX");
        BiomeGenBase[] list = BiomeGenBase.getBiomeGenArray();
        java.util.List<Class<?>> classes = new java.util.ArrayList<>();
        b.putInt(list.length);
        for (BiomeGenBase g : list) {
            if (g == null) {
                b.put((byte) 0)
                    .put((byte) 0)
                    .put((byte) 0)
                    .put((byte) 0)
                    .putInt(-1);
                continue;
            }
            Class<?> c = g.getBiomeClass();
            if (!classes.contains(c)) classes.add(c);
            int flags = (g.func_150559_j() ? 1 : 0) | (g instanceof BiomeGenMesa ? 2 : 0)
                | (BiomeManager.oceanBiomes.contains(g) ? 4 : 0)
                | (c == BiomeGenJungle.class ? 8 : 0);
            b.put((byte) 1)
                .put((byte) classes.indexOf(c))
                .put((byte) g.getTempCategory()
                    .ordinal())
                .put((byte) flags)
                .putInt(g.biomeID);
        }
        close(b, s);
    }

    /**
     * LAYR: the overworld's two GenLayer chains as a graph, parents before children: each layer's kind, mode, seeds
     * and inputs, and GenLayerBiome's weighted lists. Left out when a layer is not exactly one of LAYER_KINDS (a mod
     * changed the chain): the card then takes the biomes from the server.
     */
    private static void layers(ByteBuffer b, World world) {
        if (world.getWorldChunkManager()
            .getClass() != WorldChunkManager.class) return;
        AccessorWorldChunkManager m = (AccessorWorldChunkManager) world.getWorldChunkManager();
        java.util.IdentityHashMap<GenLayer, Integer> index = new java.util.IdentityHashMap<>();
        java.util.List<GenLayer> order = new java.util.ArrayList<>();
        if (!walk(m.phiwg$getGenBiomes(), index, order) || !walk(m.phiwg$getBiomeIndexLayer(), index, order)) return;
        int s = open(b, "LAYR");
        b.putInt(order.size())
            .putInt(index.get(m.phiwg$getGenBiomes()))
            .putInt(index.get(m.phiwg$getBiomeIndexLayer()));
        for (GenLayer l : order) {
            GenLayer[] in = inputs(l);
            AccessorGenLayer a = (AccessorGenLayer) l;
            b.put((byte) java.util.Arrays.asList(LAYER_KINDS)
                .indexOf(l.getClass()))
                .put((byte) (l instanceof GenLayerEdge e ? ((AccessorGenLayerEdge) e).phiwg$getMode()
                    .ordinal() : 0))
                .putLong(a.phiwg$getBaseSeed())
                .putLong(a.phiwg$getWorldGenSeed())
                .putInt(in.length > 0 ? index.get(in[0]) : -1)
                .putInt(in.length > 1 ? index.get(in[1]) : -1);
            if (l instanceof GenLayerBiome g) {
                java.util.List<BiomeManager.BiomeEntry>[] lists = ((AccessorGenLayerBiome) g).phiwg$getBiomes();
                for (BiomeManager.BiomeType t : BiomeManager.BiomeType.values()) {
                    java.util.List<BiomeManager.BiomeEntry> e = lists[t.ordinal()];
                    b.put((byte) (BiomeManager.isTypeListModded(t) ? 1 : 0))
                        .putInt(e.size());
                    for (BiomeManager.BiomeEntry x : e) b.putInt(x.biome.biomeID)
                        .putInt(x.itemWeight);
                }
            }
        }
        close(b, s);
    }

    /** A layer's inputs in the order getInts reads them. */
    private static GenLayer[] inputs(GenLayer l) {
        if (l instanceof GenLayerRiverMix r) {
            AccessorGenLayerRiverMix a = (AccessorGenLayerRiverMix) r;
            return new GenLayer[] { a.phiwg$getBiomes(), a.phiwg$getRivers() };
        }
        GenLayer p = ((AccessorGenLayer) l).phiwg$getParent();
        if (l instanceof GenLayerHills h) return new GenLayer[] { p, ((AccessorGenLayerHills) h).phiwg$getRiverNoise() };
        return p == null ? new GenLayer[0] : new GenLayer[] { p };
    }

    /** Post-order: every input before the layer; false if a layer is not one the card ports. */
    private static boolean walk(GenLayer l, java.util.IdentityHashMap<GenLayer, Integer> index,
        java.util.List<GenLayer> order) {
        if (index.containsKey(l)) return true;
        if (!java.util.Arrays.asList(LAYER_KINDS)
            .contains(l.getClass())) return false;
        for (GenLayer p : inputs(l)) if (!walk(p, index, order)) return false;
        index.put(l, order.size());
        order.add(l);
        return true;
    }

    public static void beginChunk(int x, int z) {
        chunk = ByteBuffer.allocate(1 << 20)
            .order(ByteOrder.LITTLE_ENDIAN);
        chunk.put("PWGC".getBytes(StandardCharsets.US_ASCII))
            .putInt(1);
        int s = open(chunk, "CHNK");
        chunk.putInt(x)
            .putInt(z);
        close(chunk, s);
        flags = 0;
        surface = null;
    }

    /** Writes c_X_Z.bin; returns {blocks the caves changed, blocks the ravines changed}. */
    public static long[] endChunk(int x, int z) throws IOException {
        int s = open(chunk, "FLAG");
        chunk.putInt(flags);
        close(chunk, s);
        Files.write(dir.resolve("c_" + x + "_" + z + ".bin"), java.util.Arrays.copyOf(chunk.array(), chunk.position()));
        chunk = null;
        long[] carved = { carvedCaves, carvedRavines };
        carvedCaves = carvedRavines = 0;
        return carved;
    }

    public static void abort() {
        chunk = null;
    }

    public static void flag(int bit) {
        flags |= bit;
    }

    public static void doubles(String tag, double[] a, int n) {
        int s = open(chunk, tag);
        for (int i = 0; i < n; i++) chunk.putDouble(a[i]);
        close(chunk, s);
    }

    public static void biomes(String tag, BiomeGenBase[] a, int n) {
        int s = open(chunk, tag);
        for (int i = 0; i < n; i++) chunk.putShort((short) (a[i] == null ? 0xFFFF : a[i].biomeID));
        close(chunk, s);
    }

    public static void bytes(String tag, byte[] a) {
        int s = open(chunk, tag);
        chunk.put(a);
        close(chunk, s);
    }

    /** Block ids, 0xFFFF for null; counts changes against the surface's array for the carvers. */
    public static void blocks(String tag, Block[] a) {
        short[] ids = new short[a.length];
        for (int i = 0; i < a.length; i++) ids[i] = a[i] == null ? (short) 0xFFFF : blockId(a[i]);
        int s = open(chunk, tag);
        for (short v : ids) chunk.putShort(v);
        close(chunk, s);
        if (tag.equals("SURF")) surface = ids;
        else if (surface != null && (tag.equals("CAVE") || tag.equals("RAVN"))) {
            long n = 0;
            for (int i = 0; i < ids.length; i++) if (ids[i] != surface[i]) n++;
            if (tag.equals("CAVE")) carvedCaves = n;
            else carvedRavines = n;
            surface = ids;
        }
    }

    private static short blockId(Block b) {
        return (short) Block.getIdFromBlock(b);
    }

    private static int open(ByteBuffer b, String tag) {
        b.put(tag.getBytes(StandardCharsets.US_ASCII))
            .putInt(0);
        return b.position();
    }

    private static void close(ByteBuffer b, int start) {
        b.putInt(start - 4, b.position() - start);
    }

    private static void refuse(String what) {
        throw new IllegalStateException("phiwg capture: not vanilla, refusing: " + what);
    }
}
