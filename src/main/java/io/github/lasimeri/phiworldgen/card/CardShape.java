package io.github.lasimeri.phiworldgen.card;

import java.io.File;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

import net.minecraft.block.Block;
import net.minecraft.init.Blocks;
import net.minecraft.world.WorldServer;
import net.minecraft.world.biome.BiomeGenBase;
import net.minecraft.world.chunk.storage.AnvilChunkLoader;
import net.minecraft.world.chunk.storage.IChunkLoader;
import net.minecraft.world.gen.ChunkProviderServer;

import io.github.lasimeri.phiworldgen.PhiWorldgen;
import io.github.lasimeri.phiworldgen.profile.GenProfile;

/** The server thread's side of the card's terrain shape: ask, prefetch, wait, decode, check. See CardShape.md. */
public final class CardShape {

    public static volatile boolean enabled = true, check = false;
    public static volatile int radius = 4, perChunk = 16;
    public static volatile long waitMs = 1;

    public static long ready, waited, waitNs, misses, late, prefetched, failures, checked, mismatched;
    public static long biomesUsed, biomesChecked, biomesMismatched, wholeUsed, wholeChecked, wholeMismatched;

    private CardShape() {}

    /**
     * The card's shape of chunk x, z as column runs, or null when the CPU must make it. The server thread never
     * waits for the card beyond waitMs: a chunk never asked for (a miss) or not answered in time (late) is made by
     * the CPU at vanilla's cost, while the ring around it is asked for, so the next chunks are the card's.
     */
    public static byte[] fetch(WorldServer world, int x, int z) {
        CardClient c = CardClient.get();
        if (c == null) return null;
        try {
            GenProfile.begin(GenProfile.PREFETCH);
            prefetch(c, world, x, z);
            GenProfile.end(GenProfile.PREFETCH);
            CardClient.Entry e = c.take(CardClient.key(x, z));
            if (e == null) {
                misses++;
                return null;
            }
            if (e.answer.isDone()) {
                byte[] rle = e.answer.getNow(null);
                ready++;
                return rle;
            }
            long t0 = System.nanoTime();
            try {
                byte[] rle = e.answer.get(waitMs, TimeUnit.MILLISECONDS);
                waited++;
                waitNs += System.nanoTime() - t0;
                return rle;
            } catch (TimeoutException ex) {
                late++;
                return null;
            }
        } catch (Exception ex) {
            failures++;
            return null;
        }
    }

    /** Asks for every chunk within radius of x, z that is neither loaded, nor on disk, nor already asked. */
    private static void prefetch(CardClient c, WorldServer world, int x, int z) throws java.io.IOException {
        if (c.pendingCount() > 4096) c.evictOlderThan(TimeUnit.SECONDS.toNanos(60));
        if (c.pendingCount() > 4096) return;
        ChunkProviderServer cps = world.theChunkProviderServer;
        IChunkLoader loader = cps.currentChunkLoader;
        File regions = new File(
            world.getSaveHandler()
                .getWorldDirectory(),
            "region");
        int asked = 0;
        rings: for (int r = 1; r <= radius; r++) {
            for (int dx = -r; dx <= r; dx++) {
                for (int dz = -r; dz <= r; dz++) {
                    if (Math.max(Math.abs(dx), Math.abs(dz)) != r) continue;
                    int nx = x + dx, nz = z + dz;
                    if (c.pending(CardClient.key(nx, nz)) != null || cps.chunkExists(nx, nz)) continue;
                    // look for the region file first: AnvilChunkLoader.chunkExists would create a missing one
                    File region = new File(regions, "r." + (nx >> 5) + "." + (nz >> 5) + ".mca");
                    if (region.exists() && loader instanceof AnvilChunkLoader
                        && ((AnvilChunkLoader) loader).chunkExists(world, nx, nz)) continue;
                    // with the layers on the card, coordinates only: no GenLayer work on the server thread
                    BiomeGenBase[] b = c.cardBiomes ? null
                        : world.getWorldChunkManager()
                            .getBiomesForGeneration(null, nx * 4 - 2, nz * 4 - 2, 10, 10);
                    c.request(nx, nz, b, false);
                    if (++asked >= perChunk) break rings;
                }
            }
        }
        if (asked > 0) {
            c.flush();
            prefetched += asked;
        }
    }

    /** An answer's payload (protocol 2): u32 flags; with flag 1, u16[256] biome slots; then the column runs. */
    public static boolean hasBiomes(byte[] a) {
        return (a[0] & 1) != 0;
    }

    private static int runsAt(byte[] a) {
        return hasBiomes(a) ? 4 + 512 : 4;
    }

    private static int slot(byte[] a, int i) {
        return (a[4 + 2 * i] & 0xFF) | (a[5 + 2 * i] & 0xFF) << 8;
    }

    /** The card's 16 x 16 biomes as loadBlockGeneratorData returns them: getBiomeGenArray()[slot], the same objects. */
    public static BiomeGenBase[] biomes(byte[] a, BiomeGenBase[] list) {
        if (list == null || list.length < 256) list = new BiomeGenBase[256];
        BiomeGenBase[] all = BiomeGenBase.getBiomeGenArray();
        for (int i = 0; i < 256; i++) {
            int s = slot(a, i);
            list[i] = s == 0xFFFF ? null : all[s];
        }
        return list;
    }

    /** Check mode: the card's 16 x 16 biomes against vanilla's, object for object. */
    public static void compareBiomes(int x, int z, byte[] a, BiomeGenBase[] vanilla) {
        BiomeGenBase[] all = BiomeGenBase.getBiomeGenArray();
        biomesChecked++;
        for (int i = 0; i < 256; i++) {
            int s = slot(a, i);
            BiomeGenBase card = s == 0xFFFF ? null : all[s];
            if (card != vanilla[i]) {
                if (biomesMismatched++ < 10) PhiWorldgen.LOG.warn(
                    "phiwg card check: chunk {},{} biome x{} z{}: card {}, vanilla {}",
                    x,
                    z,
                    i & 15,
                    i >> 4,
                    card == null ? "null" : card.biomeName,
                    vanilla[i] == null ? "null" : vanilla[i].biomeName);
                return;
            }
        }
    }

    /** Answer flag 2 (protocol 3): the whole chunk, surface, caves and ravines included. */
    public static boolean isWhole(byte[] a) {
        return (a[0] & 2) != 0;
    }

    private static int u16(byte[] a, int p) {
        return (a[p] & 0xFF) | (a[p + 1] & 0xFF) << 8;
    }

    private static final Block[] BY_ID = new Block[4096];

    private static Block block(int id) {
        if (id == 0xFFFF) return null;
        Block b = BY_ID[id];
        if (b == null) BY_ID[id] = b = Block.getBlockById(id);
        return b;
    }

    /** A whole answer into provideChunk's arrays: per column (x << 4 | z) from y 0, runs of (u16 id, u8 meta, u8 n-1). */
    public static void decodeWhole(byte[] a, Block[] blocks, byte[] metas) {
        int p = runsAt(a);
        p += 2 + 8 * u16(a, p);
        int c = 0, y = 0;
        while (c < 256 && p + 4 <= a.length) {
            Block b = block(u16(a, p));
            byte m = a[p + 2];
            int run = (a[p + 3] & 0xFF) + 1, base = c << 8;
            for (int k = 0; k < run; k++, y++) {
                blocks[base | y] = b;
                metas[base | y] = m;
            }
            p += 4;
            if (y == 256) {
                y = 0;
                c++;
            }
        }
    }

    /**
     * The end state of the card's surface: the fields its overrides (Hills, Savanna M, mega Taiga) left on these
     * biome objects, set on the server's, as vanilla's surface would have.
     */
    public static void applyStates(byte[] a) {
        int p = runsAt(a), n = u16(a, p);
        BiomeGenBase[] all = BiomeGenBase.getBiomeGenArray();
        for (int i = 0, q = p + 2; i < n; i++, q += 8) {
            BiomeGenBase g = all[u16(a, q)];
            if (g == null) continue;
            g.topBlock = block(u16(a, q + 2));
            g.fillerBlock = block(u16(a, q + 4));
            g.field_150604_aj = a[q + 6] & 0xFF;
        }
    }

    /** Check mode: the card's whole chunk against the chunk vanilla made, and its end states against the objects. */
    public static void compareWhole(int x, int z, byte[] a, net.minecraft.world.chunk.Chunk ch) {
        wholeChecked++;
        int p = runsAt(a), n = u16(a, p);
        BiomeGenBase[] all = BiomeGenBase.getBiomeGenArray();
        for (int i = 0, q = p + 2; i < n; i++, q += 8) {
            BiomeGenBase g = all[u16(a, q)];
            if (g != null && (g.topBlock != block(u16(a, q + 2)) || g.fillerBlock != block(u16(a, q + 4))
                || (g.field_150604_aj & 255) != (a[q + 6] & 0xFF))) {
                if (wholeMismatched++ < 10) PhiWorldgen.LOG
                    .warn("phiwg card check: chunk {},{}: end state of biome slot {} differs", x, z, u16(a, q));
                return;
            }
        }
        Block[] blocks = new Block[65536];
        byte[] metas = new byte[65536];
        decodeWhole(a, blocks, metas);
        for (int i = 0; i < 65536; i++) {
            int bx = i >> 12, bz = (i >> 8) & 15, by = i & 255;
            Block card = blocks[i];
            boolean air = card == null || card.getMaterial() == net.minecraft.block.material.Material.air;
            Block made = ch.getBlock(bx, by, bz);
            boolean ok = air ? made == Blocks.air
                : made == card && ch.getBlockMetadata(bx, by, bz) == (metas[i] & 15);
            if (!ok) {
                if (wholeMismatched++ < 10) PhiWorldgen.LOG.warn(
                    "phiwg card check: chunk {},{} block x{} z{} y{}: card {}:{}, vanilla {}:{}",
                    x,
                    z,
                    bx,
                    bz,
                    by,
                    card,
                    metas[i],
                    made,
                    ch.getBlockMetadata(bx, by, bz));
                return;
            }
        }
    }

    /** Column runs into the block array, as func_147424_a would have filled it: stone, water or null. */
    public static void decode(byte[] rle, Block[] blocks) {
        int c = 0, y = 0;
        for (int p = runsAt(rle); p + 1 < rle.length; p += 2) {
            Block b = rle[p] == 1 ? Blocks.stone : rle[p] == 2 ? Blocks.water : null;
            int base = c << 8, run = (rle[p + 1] & 0xFF) + 1;
            for (int k = 0; k < run; k++) blocks[base | y++] = b;
            if (y == 256) {
                y = 0;
                c++;
            }
        }
    }

    /** Check mode: the card's answer against the block array vanilla just made. */
    public static void compare(int x, int z, byte[] rle, Block[] vanilla) {
        Block[] card = new Block[65536];
        decode(rle, card);
        checked++;
        for (int i = 0; i < 65536; i++) {
            if (card[i] != vanilla[i]) {
                if (mismatched++ < 10) PhiWorldgen.LOG.warn(
                    "phiwg card check: chunk {},{} block x{} z{} y{}: card {}, vanilla {}",
                    x,
                    z,
                    i >> 12,
                    (i >> 8) & 15,
                    i & 255,
                    card[i],
                    vanilla[i]);
                return;
            }
        }
    }

    public static String status() {
        CardClient c = CardClient.get();
        return String.format(
            "phiwg card: %s, %s%s; card shapes used %d (ready %d, waited for %d at %.3f ms average), CPU shapes %d (never asked %d, late %d), prefetched %d, failures %d; card biomes used %d, whole chunks used %d; check %s: %d chunks, %d differ, biomes %d chunks, %d differ, whole chunks %d, %d differ%s",
            enabled ? "on" : "off",
            c == null ? "not connected"
                : "connected through the " + c.linkName()
                    + " link to "
                    + c.workers
                    + " card workers, "
                    + c.answers.get()
                    + " answers, "
                    + c.bytes.get() / 1024
                    + " KiB, "
                    + String.format("%.2f", c.answers.get() == 0 ? 0.0 : c.computeMicros.get() / 1e3 / c.answers.get())
                    + " ms card compute each",
            c == null ? "" : ", " + c.pendingCount() + " pending",
            ready + waited,
            ready,
            waited,
            waited == 0 ? 0.0 : waitNs / 1e6 / waited,
            misses + late,
            misses,
            late,
            prefetched,
            failures,
            biomesUsed,
            wholeUsed,
            check ? "on" : "off",
            checked,
            mismatched,
            biomesChecked,
            biomesMismatched,
            wholeChecked,
            wholeMismatched,
            "");
    }

    public static void resetStats() {
        ready = waited = waitNs = misses = late = prefetched = failures = checked = mismatched = 0;
        biomesUsed = biomesChecked = biomesMismatched = wholeUsed = wholeChecked = wholeMismatched = 0;
    }
}
