package io.github.lasimeri.phiworldgen.card;

import java.io.IOException;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.Field;
import java.nio.ByteOrder;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.concurrent.locks.LockSupport;

import net.minecraft.world.biome.BiomeGenBase;

/**
 * pwgd through the shared host-memory region (docs/shm-protocol.md): the file phictl pins for the card through vfio,
 * written by the card's DMA engine. See ShmLink.md.
 */
final class ShmLink implements CardClient.Link {

    private static final int S_MAGIC = 0, S_VERSION = 4, S_SESSION = 8, S_WLEN = 16, S_POSTED = 64, S_COMPLETED = 128,
        S_CSESSION = 192, S_CWORKERS = 200, S_CONSUMED = 256, S_ALIVE = 320;
    private static final int S_WORLD = 64 << 10, S_WORLD_MAX = (2 << 20) - S_WORLD, S_REQ = 2 << 20, S_REQ_N = 4096,
        S_REQ_SIZE = 256, S_COMPL = 3 << 20, S_SLOTS = 4 << 20, S_SLOT_N = 2048, S_SLOT_SIZE = 135168;
    private static final long PART = S_SLOTS + (long) S_SLOT_N * S_SLOT_SIZE;

    /* sun.misc.Unsafe's fences through method handles: no compile-time dependency, and they inline */
    private static final MethodHandle STORE_FENCE, LOAD_FENCE;

    static {
        try {
            Class<?> u = Class.forName("sun.misc.Unsafe");
            Field f = u.getDeclaredField("theUnsafe");
            f.setAccessible(true);
            Object unsafe = f.get(null);
            MethodHandles.Lookup l = MethodHandles.lookup();
            STORE_FENCE = l.findVirtual(u, "storeFence", MethodType.methodType(void.class))
                .bindTo(unsafe);
            LOAD_FENCE = l.findVirtual(u, "loadFence", MethodType.methodType(void.class))
                .bindTo(unsafe);
        } catch (ReflectiveOperationException e) {
            throw new ExceptionInInitializerError(e);
        }
    }

    private final FileChannel channel;
    private final MappedByteBuffer m;
    private long session, posted;
    private volatile long completed;
    private volatile boolean open = true;

    private ShmLink(FileChannel channel, MappedByteBuffer m) {
        this.channel = channel;
        this.m = m;
    }

    private static void storeFence() {
        try {
            STORE_FENCE.invokeExact();
        } catch (Throwable t) {
            throw new IllegalStateException(t);
        }
    }

    private static void loadFence() {
        try {
            LOAD_FENCE.invokeExact();
        } catch (Throwable t) {
            throw new IllegalStateException(t);
        }
    }

    /** Maps the part of file at offset, opens a session with the world state, and returns the client. */
    static CardClient connect(Path file, long offset, byte[] world, Object provider) throws IOException {
        if (world.length > S_WORLD_MAX) throw new IOException("world state of " + world.length + " bytes is too long");
        FileChannel ch = FileChannel.open(file, StandardOpenOption.READ, StandardOpenOption.WRITE);
        if (ch.size() < offset + PART) {
            ch.close();
            throw new IOException(file + " is " + ch.size() + " bytes, too short for the part at " + offset);
        }
        MappedByteBuffer m = ch.map(FileChannel.MapMode.READ_WRITE, offset, PART);
        m.order(ByteOrder.LITTLE_ENDIAN);
        ShmLink link = new ShmLink(ch, m);
        try {
            long beat = m.getLong(S_ALIVE);
            sleep(300);
            if (m.getLong(S_ALIVE) == beat) throw new IOException("no pwgd heartbeat in the shared region");
            for (int i = 0; i < S_SLOT_N; i++) m.putLong(S_COMPL + i * 64, 0);
            m.putLong(S_POSTED, 0);
            m.putLong(S_COMPLETED, 0);
            for (int i = 0; i < world.length; i++) m.put(S_WORLD + i, world[i]);
            m.putInt(S_WLEN, world.length);
            m.putInt(S_VERSION, CardClient.PROTOCOL);
            byte[] magic = "PWGS".getBytes(StandardCharsets.US_ASCII);
            for (int i = 0; i < 4; i++) m.put(S_MAGIC + i, magic[i]);
            storeFence();
            long s = m.getLong(S_SESSION) + 1;
            if (s == 0) s = 1;
            m.putLong(S_SESSION, s);
            link.session = s;
            long t0 = System.nanoTime();
            while (m.getLong(S_CSESSION) != s) {
                if (System.nanoTime() - t0 > 5_000_000_000L)
                    throw new IOException("pwgd did not take the session in 5 s");
                sleep(1);
            }
            loadFence();
            CardClient c = new CardClient(m.getInt(S_CWORKERS), provider);
            Thread t = new Thread(() -> link.read(c), "phiwg-card-shm");
            t.setDaemon(true);
            t.start();
            return CardClient.install(c, link);
        } catch (IOException e) {
            link.close();
            throw e;
        }
    }

    private static void sleep(long ms) throws IOException {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException e) {
            throw new IOException("interrupted");
        }
    }

    @Override
    public boolean send(int cx, int cz, BiomeGenBase[] bi10, boolean urgent) {
        if (posted - m.getLong(S_CONSUMED) >= S_REQ_N || posted - completed >= S_SLOT_N) return false;
        int e = S_REQ + (int) (posted % S_REQ_N) * S_REQ_SIZE;
        m.putInt(e, cx);
        m.putInt(e + 4, cz);
        // flags: 1 urgent, 2 the card computes the biomes (bi10 null: the request's are zero)
        m.putInt(e + 8, (urgent ? 1 : 0) | (bi10 == null ? 2 : 0));
        for (int i = 0; i < 100; i++) m.putShort(e + 12 + 2 * i, bi10 == null ? 0 : (short) bi10[i].biomeID);
        posted++;
        if (urgent) flush();
        return true;
    }

    @Override
    public void flush() {
        storeFence();
        m.putLong(S_POSTED, posted);
    }

    @Override
    public void close() {
        open = false;
        try {
            channel.close();
        } catch (IOException ignored) {}
    }

    @Override
    public String name() {
        return "shared-region";
    }

    /** Answers in order of completion entries; fails the client when pwgd's heartbeat stops or its session moves. */
    private void read(CardClient c) {
        long lastBeat = m.getLong(S_ALIVE), beatSeen = System.nanoTime();
        while (open) {
            int tag = S_COMPL + (int) (completed % S_SLOT_N) * 64;
            if (m.getLong(tag) == completed + 1) {
                loadFence();
                int slot = S_SLOTS + (int) (completed % S_SLOT_N) * S_SLOT_SIZE;
                int cx = m.getInt(slot), cz = m.getInt(slot + 4), len = m.getInt(slot + 8), us = m.getInt(slot + 12);
                byte[] rle = new byte[len];
                for (int i = 0; i < len; i++) rle[i] = m.get(slot + 64 + i);
                completed++;
                m.putLong(S_COMPLETED, completed);
                c.deliver(cx, cz, rle, us);
                continue;
            }
            long now = System.nanoTime(), beat = m.getLong(S_ALIVE);
            if (beat != lastBeat) {
                lastBeat = beat;
                beatSeen = now;
            } else if (now - beatSeen > 2_000_000_000L) {
                c.fail(new IOException("pwgd heartbeat stopped"));
                return;
            }
            if (m.getLong(S_CSESSION) != session) {
                c.fail(new IOException("pwgd moved to another session"));
                return;
            }
            // 50 us while answers are awaited; 1 ms when nothing is (the poll cost 2.7 % of a core idle, 2026-10-22)
            LockSupport.parkNanos(c.pendingCount() > 0 ? 50_000 : 1_000_000);
        }
    }
}
