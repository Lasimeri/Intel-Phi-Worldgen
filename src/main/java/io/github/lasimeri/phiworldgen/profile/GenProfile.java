package io.github.lasimeri.phiworldgen.profile;

import java.util.LinkedHashSet;
import java.util.Set;

/** Time spent in each phase of overworld chunk generation, summed since the last reset. See GenProfile.md. */
public final class GenProfile {

    public static final int PROVIDE = 0, SHAPE = 1, BIOMES10 = 2, NOISE = 3, OCTAVES = 4, INTERP = 5, CARD = 6,
        PREFETCH = 7, BIOMES16 = 8, SURFACE = 9, CAVES = 10, RAVINES = 11, STRUCTURES = 12, CHUNK = 13, SKYLIGHT = 14,
        POPULATE = 15;
    private static final String[] NAMES = { "provideChunk", "  shape (func_147424_a)",
        "    biomes 10x10 (getBiomesForGeneration)", "    noise field (func_147423_a)",
        "      octaves (4 x generateNoiseOctaves)", "    interpolation to blocks", "  card shape (prefetch + take)",
        "    prefetch (biomes, requests)", "  biomes 16x16 (loadBlockGeneratorData)",
        "  surface (replaceBlocksForBiome)", "  caves (MapGenCaves)", "  ravines (MapGenRavine)",
        "  structures (4 x MapGenStructure)", "  new Chunk", "  generateSkylightMap", "populate" };
    private static final int N = NAMES.length;

    private static final long[] start = new long[N];
    private static final long[] total = new long[N];
    private static final long[] max = new long[N];
    private static final long[] maxAt = new long[N];
    private static final long[] calls = new long[N];
    private static final boolean[] open = new boolean[N];
    private static final Set<String> threads = new LinkedHashSet<>();

    private GenProfile() {}

    public static void begin(int phase) {
        if (phase == PROVIDE || phase == POPULATE) threads.add(
            Thread.currentThread()
                .getName());
        start[phase] = System.nanoTime();
        open[phase] = true;
    }

    /** Ends a phase begun since its last end; an end without a begin (a cancelled method) is ignored. */
    public static void end(int phase) {
        if (!open[phase]) return;
        open[phase] = false;
        long ns = System.nanoTime() - start[phase];
        total[phase] += ns;
        if (ns > max[phase]) {
            max[phase] = ns;
            maxAt[phase] = calls[phase];
        }
        calls[phase]++;
    }

    public static boolean isOpen(int phase) {
        return open[phase];
    }

    public static void endIfOpen(int phase) {
        end(phase);
    }

    public static long chunks() {
        return calls[PROVIDE];
    }

    public static void reset() {
        java.util.Arrays.fill(total, 0);
        java.util.Arrays.fill(max, 0);
        java.util.Arrays.fill(maxAt, 0);
        java.util.Arrays.fill(calls, 0);
        java.util.Arrays.fill(open, false);
        threads.clear();
    }

    /** One line per phase: total ms, calls, ms per call, the longest call, share of provideChunk + populate. */
    public static String report(String title) {
        StringBuilder sb = new StringBuilder();
        double whole = (total[PROVIDE] + total[POPULATE]) / 1e6;
        sb.append(
            String.format(
                "== %s: %d chunks generated, %d populated, threads %s%n",
                title,
                calls[PROVIDE],
                calls[POPULATE],
                threads));
        for (int p = 0; p < N; p++) {
            double ms = total[p] / 1e6;
            sb.append(
                String.format(
                    "%-46s %10.1f ms %7d calls %8.3f ms/call %8.3f ms max (call %d) %6.2f %%%n",
                    NAMES[p],
                    ms,
                    calls[p],
                    calls[p] == 0 ? 0.0 : ms / calls[p],
                    max[p] / 1e6,
                    maxAt[p],
                    whole == 0 ? 0.0 : 100.0 * ms / whole));
        }
        return sb.toString();
    }
}
