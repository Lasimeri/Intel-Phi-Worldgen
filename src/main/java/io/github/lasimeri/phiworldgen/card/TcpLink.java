package io.github.lasimeri.phiworldgen.card;

import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;

import net.minecraft.world.biome.BiomeGenBase;

/**
 * pwgd over TCP (card/pwgd/pwgd.md): the fallback when the shared host-memory region is unavailable. See TcpLink.md.
 */
final class TcpLink implements CardClient.Link {

    private final Socket socket;
    private final OutputStream out;
    private final DataInputStream in;

    private TcpLink(Socket socket) throws IOException {
        this.socket = socket;
        this.out = new BufferedOutputStream(socket.getOutputStream(), 1 << 16);
        this.in = new DataInputStream(socket.getInputStream());
    }

    /** Opens host:port, sends the world state, and returns the client reading its answers. */
    static CardClient connect(String host, int port, byte[] world, Object provider) throws IOException {
        Socket s = new Socket();
        s.connect(new InetSocketAddress(host, port), 3000);
        s.setTcpNoDelay(true);
        s.setSoTimeout(30000);
        ByteBuffer hello = ByteBuffer.allocate(12)
            .order(ByteOrder.LITTLE_ENDIAN);
        hello.put("PWGH".getBytes(StandardCharsets.US_ASCII))
            .putInt(CardClient.PROTOCOL)
            .putInt(world.length);
        OutputStream o = s.getOutputStream();
        o.write(hello.array());
        o.write(world);
        o.flush();
        byte[] ready = new byte[12];
        new DataInputStream(s.getInputStream()).readFully(ready);
        ByteBuffer r = ByteBuffer.wrap(ready)
            .order(ByteOrder.LITTLE_ENDIAN);
        if (!new String(ready, 0, 4, StandardCharsets.US_ASCII).equals("PWGR") || r.getInt(4) != CardClient.PROTOCOL) {
            s.close();
            throw new IOException("pwgd answered without PWGR version " + CardClient.PROTOCOL);
        }
        s.setSoTimeout(0);
        TcpLink link = new TcpLink(s);
        CardClient c = new CardClient(r.getInt(8), provider);
        Thread t = new Thread(() -> link.read(c), "phiwg-card-tcp");
        t.setDaemon(true);
        t.start();
        return CardClient.install(c, link);
    }

    @Override
    public boolean send(int cx, int cz, BiomeGenBase[] bi10, boolean urgent, boolean whole) throws IOException {
        ByteBuffer q = ByteBuffer.allocate(212)
            .order(ByteOrder.LITTLE_ENDIAN);
        q.putInt(cx)
            .putInt(cz)
            .putInt((urgent ? 1 : 0) | (bi10 == null ? 2 : 0) | (whole ? 4 : 0));
        for (int i = 0; i < 100; i++) q.putShort(bi10 == null ? 0 : (short) bi10[i].biomeID);
        synchronized (out) {
            out.write(q.array());
            if (urgent) out.flush();
        }
        return true;
    }

    @Override
    public void flush() throws IOException {
        synchronized (out) {
            out.flush();
        }
    }

    @Override
    public void close() {
        try {
            socket.close();
        } catch (IOException ignored) {}
    }

    @Override
    public String name() {
        return "tcp";
    }

    private void read(CardClient c) {
        byte[] h = new byte[16];
        try {
            for (;;) {
                in.readFully(h);
                ByteBuffer b = ByteBuffer.wrap(h)
                    .order(ByteOrder.LITTLE_ENDIAN);
                int cx = b.getInt(), cz = b.getInt(), len = b.getInt(), us = b.getInt();
                byte[] rle = new byte[len];
                in.readFully(rle);
                c.deliver(cx, cz, rle, us);
            }
        } catch (IOException ex) {
            c.fail(ex);
        }
    }
}
