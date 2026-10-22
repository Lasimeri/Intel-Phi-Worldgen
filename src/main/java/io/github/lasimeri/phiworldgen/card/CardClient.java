package io.github.lasimeri.phiworldgen.card;

import java.io.IOException;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicLong;

import net.minecraft.world.biome.BiomeGenBase;

import io.github.lasimeri.phiworldgen.PhiWorldgen;
import io.github.lasimeri.phiworldgen.capture.Capture;

/** The mod's side of the card's pwgd: pending requests, answers, the link in use. See CardClient.md. */
public final class CardClient {

    /** How requests reach pwgd and answers come back: the shared host-memory region, or TCP. */
    public interface Link {

        /** Queues one request; false when the link has no room now (the caller lets the CPU make the chunk). */
        boolean send(int cx, int cz, BiomeGenBase[] bi10, boolean urgent, boolean whole) throws IOException;

        /** Makes the queued requests visible to the card. */
        void flush() throws IOException;

        void close();

        String name();
    }

    /** A chunk asked of the card: its answer and when it was asked. */
    public static final class Entry {

        public final CompletableFuture<byte[]> answer = new CompletableFuture<>();
        final long asked = System.nanoTime();
    }

    private static volatile CardClient current;

    private final ConcurrentHashMap<Long, Entry> pending = new ConcurrentHashMap<>();
    private volatile boolean up = true;
    private Link link;
    /** docs/shm-protocol.md: requests and answers of version 2. */
    public static final int PROTOCOL = 3;
    public final int workers;
    /** The world state holds the GenLayer chains (LAYR): the card computes every chunk's biomes from its coordinates. */
    public volatile boolean cardBiomes;
    /** The card makes whole chunks (surface, caves, ravines): the tables are in the world state and no mod hooks
     * ReplaceBiomeBlocks. */
    public volatile boolean cardWhole;
    /** The generator whose state the card was given: the only one it may serve. */
    public final Object provider;
    public final AtomicLong answers = new AtomicLong(), bytes = new AtomicLong(), computeMicros = new AtomicLong();

    CardClient(int workers, Object provider) {
        this.workers = workers;
        this.provider = provider;
        // Load and initialise Entry and CompletableFuture here, on the connecting thread: their first use cost the
        // server thread 137 ms (measured 2026-10-22, the first chunk after a connect).
        new Entry().answer.complete(null);
    }

    /** The connected client, or null. */
    public static CardClient get() {
        CardClient c = current;
        return c != null && c.up ? c : null;
    }

    public static long key(int cx, int cz) {
        return ((long) cx << 32) ^ (cz & 0xFFFFFFFFL);
    }

    /**
     * Connects to pwgd: transport "shm" (the shared host-memory region only), "tcp" (only), or "auto" (the region,
     * else TCP). world is Capture.worldBlob of provider.
     */
    public static CardClient connect(String transport, java.nio.file.Path shmFile, long shmOffset, String host,
        int port, byte[] world, Object provider) throws IOException {
        IOException shmFailure = null;
        if (!transport.equals("tcp")) {
            try {
                CardClient c = ShmLink.connect(shmFile, shmOffset, world, provider);
                c.cardBiomes = Capture.hasSection(world, "LAYR");
                c.cardWhole = wholeOk(world);
                return c;
            } catch (IOException e) {
                if (transport.equals("shm")) throw e;
                shmFailure = e;
            }
        }
        CardClient c = TcpLink.connect(host, port, world, provider);
        c.cardBiomes = Capture.hasSection(world, "LAYR");
        c.cardWhole = wholeOk(world);
        if (shmFailure != null)
            PhiWorldgen.LOG.info("phiwg card: shared region not used ({}); TCP instead", shmFailure.getMessage());
        return c;
    }

    /** Whole chunks need the layer, surface and carver tables, and no ReplaceBiomeBlocks listener anywhere. */
    static boolean wholeOk(byte[] world) {
        return Capture.hasSection(world, "LAYR") && Capture.hasSection(world, "SURB") && Capture.hasSection(world, "SNOI")
            && Capture.hasSection(world, "CARV") && !surfaceHooked();
    }

    /** Whether any handler listens to ChunkProviderEvent.ReplaceBiomeBlocks (or a parent event) on Forge's bus. */
    static boolean surfaceHooked() {
        try {
            java.lang.reflect.Field f = cpw.mods.fml.common.eventhandler.EventBus.class.getDeclaredField("busID");
            f.setAccessible(true);
            int id = f.getInt(net.minecraftforge.common.MinecraftForge.EVENT_BUS);
            net.minecraftforge.event.terraingen.ChunkProviderEvent.ReplaceBiomeBlocks e = new net.minecraftforge.event.terraingen.ChunkProviderEvent.ReplaceBiomeBlocks(
                null, 0, 0, null, null, null, null);
            for (cpw.mods.fml.common.eventhandler.IEventListener l : e.getListenerList()
                .getListeners(id)) if (!(l instanceof cpw.mods.fml.common.eventhandler.EventPriority)) return true;
            return false;
        } catch (ReflectiveOperationException | RuntimeException ex) {
            return true; // unknown: the surface stays on the server
        }
    }

    /** Makes c, already linked, the client in use. */
    static CardClient install(CardClient c, Link link) {
        c.link = link;
        close();
        current = c;
        return c;
    }

    public String linkName() {
        return link == null ? "none" : link.name();
    }

    public static void close() {
        CardClient c = current;
        current = null;
        if (c != null) c.fail(null);
    }

    public Entry pending(long key) {
        return pending.get(key);
    }

    public Entry take(long key) {
        return pending.remove(key);
    }

    public int pendingCount() {
        return pending.size();
    }

    /** Forgets entries asked longer ago than ns (the player went elsewhere); answers to them are dropped. */
    public void evictOlderThan(long ns) {
        long now = System.nanoTime();
        pending.entrySet()
            .removeIf(e -> now - e.getValue().asked > ns);
    }

    /**
     * Asks the card for chunk cx, cz unless it already has; bi10 is the chunk's 10 x 10 GenLayer biomes (the first
     * 100 entries). Returns the entry, or null when the link had no room.
     */
    public Entry request(int cx, int cz, BiomeGenBase[] bi10, boolean urgent) throws IOException {
        long k = key(cx, cz);
        Entry e = pending.get(k);
        if (e != null) return e;
        e = new Entry();
        if (!link.send(cx, cz, bi10, urgent, cardWhole)) return null;
        pending.put(k, e);
        return e;
    }

    public void flush() throws IOException {
        link.flush();
    }

    /** From the link's reader: one answer. */
    void deliver(int cx, int cz, byte[] rle, int micros) {
        answers.incrementAndGet();
        bytes.addAndGet(rle.length);
        computeMicros.addAndGet(micros);
        Entry e = pending.get(key(cx, cz));
        if (e != null) e.answer.complete(rle);
    }

    /** From the link (an I/O failure) or close() (null): every pending request fails, the CPU takes over. */
    void fail(IOException ex) {
        if (!up) return;
        up = false;
        if (current == this) current = null;
        if (ex != null) PhiWorldgen.LOG
            .warn("phiwg card: {} link lost ({}); generation falls back to the CPU", linkName(), ex.toString());
        else PhiWorldgen.LOG.info("phiwg card: {} link closed", linkName());
        for (Entry e : pending.values()) e.answer.completeExceptionally(ex != null ? ex : new IOException("closed"));
        pending.clear();
        if (link != null) link.close();
    }
}
