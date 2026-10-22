package io.github.lasimeri.phiworldgen.command;

import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import net.minecraft.command.CommandBase;
import net.minecraft.command.CommandException;
import net.minecraft.command.ICommandSender;
import net.minecraft.command.WrongUsageException;
import net.minecraft.server.MinecraftServer;
import net.minecraft.util.ChatComponentText;
import net.minecraft.util.ChunkCoordinates;
import net.minecraft.world.WorldServer;
import net.minecraft.world.biome.BiomeGenBase;
import net.minecraft.world.gen.ChunkProviderGenerate;

import io.github.lasimeri.phiworldgen.PhiWorldgen;
import io.github.lasimeri.phiworldgen.capture.Capture;
import io.github.lasimeri.phiworldgen.capture.CaptureVerify;
import io.github.lasimeri.phiworldgen.card.CardShape;
import io.github.lasimeri.phiworldgen.profile.GenProfile;

/** /phiwg pregen | report | reset | capture | verify. See CommandPhiwg.md. */
public class CommandPhiwg extends CommandBase {

    @Override
    public String getCommandName() {
        return "phiwg";
    }

    @Override
    public String getCommandUsage(ICommandSender sender) {
        return "/phiwg pregen <radius> [dx dz] | report | reset | capture <name> [biome radius] | verify <name> | card [status|on|off|check on|check off|reset|radius N]";
    }

    @Override
    public int getRequiredPermissionLevel() {
        return 2;
    }

    @Override
    public void processCommand(ICommandSender sender, String[] args) {
        if (args.length == 0) throw new WrongUsageException(getCommandUsage(sender));
        switch (args[0]) {
            case "pregen" -> pregen(sender, args);
            case "report" -> send(sender, GenProfile.report("since the last reset"));
            case "reset" -> {
                GenProfile.reset();
                send(sender, "phiwg: profile reset");
            }
            case "capture" -> capture(sender, args);
            case "verify" -> verify(sender, args);
            case "card" -> card(sender, args);
            default -> throw new WrongUsageException(getCommandUsage(sender));
        }
    }

    /** Generates a (2r+1)^2 square of chunks centred dx, dz chunks from the spawn chunk, on this thread. */
    private void pregen(ICommandSender sender, String[] args) {
        if (args.length != 2 && args.length != 4) throw new WrongUsageException(getCommandUsage(sender));
        int r = parseIntBounded(sender, args[1], 0, 256);
        int dx = args.length == 4 ? parseInt(sender, args[2]) : 64;
        int dz = args.length == 4 ? parseInt(sender, args[3]) : 0;
        WorldServer world = overworld();
        ChunkCoordinates spawn = world.getSpawnPoint();
        int cx0 = (spawn.posX >> 4) + dx, cz0 = (spawn.posZ >> 4) + dz;

        GenProfile.reset();
        long t0 = System.nanoTime();
        for (int cx = cx0 - r; cx <= cx0 + r; cx++) {
            for (int cz = cz0 - r; cz <= cz0 + r; cz++) {
                world.theChunkProviderServer.loadChunk(cx, cz);
            }
        }
        double s = (System.nanoTime() - t0) / 1e9;
        long n = GenProfile.chunks();
        send(
            sender,
            String.format(
                "phiwg pregen: centre chunk %d,%d radius %d: %d chunks generated in %.3f s, %.1f chunks/s (seed %d)%n%s",
                cx0,
                cz0,
                r,
                n,
                s,
                n / s,
                world.getSeed(),
                GenProfile.report("pregen")));
    }

    /** Runs the generator on the capture set (chunkSet) with Capture on; nothing enters the world. */
    private void capture(ICommandSender sender, String[] args) {
        if (args.length != 2 && args.length != 3) throw new WrongUsageException(getCommandUsage(sender));
        int radius = args.length == 3 ? parseIntBounded(sender, args[2], 0, 4096) : 256;
        WorldServer world = overworld();
        if (!(world.theChunkProviderServer.currentChunkProvider instanceof ChunkProviderGenerate p))
            throw new CommandException("phiwg capture: the overworld generator is not ChunkProviderGenerate");
        Path dir = captureDir(args[1]);
        StringBuilder found = new StringBuilder();
        List<int[]> set = chunkSet(world, radius, found);
        long caves = 0, ravines = 0, cavesChunks = 0, ravinesChunks = 0;
        try {
            Capture.start(dir, world, p);
            for (int[] c : set) {
                Capture.beginChunk(c[0], c[1]);
                try {
                    p.provideChunk(c[0], c[1]);
                } catch (RuntimeException e) {
                    Capture.abort();
                    throw e;
                }
                long[] carved = Capture.endChunk(c[0], c[1]);
                caves += carved[0];
                ravines += carved[1];
                if (carved[0] > 0) cavesChunks++;
                if (carved[1] > 0) ravinesChunks++;
            }
        } catch (IOException | IllegalStateException e) {
            throw new CommandException("phiwg capture: " + e.getMessage());
        }
        send(
            sender,
            String.format(
                "phiwg capture %s: %d chunks to %s; caves changed %d blocks in %d chunks, ravines %d in %d; biome ids found within %d chunks: %s",
                args[1],
                set.size(),
                dir,
                caves,
                cavesChunks,
                ravines,
                ravinesChunks,
                radius,
                found));
    }

    /** The card: status, on or off, check mode, statistics reset, prefetch radius. */
    private void card(ICommandSender sender, String[] args) {
        String what = args.length > 1 ? args[1] : "status";
        switch (what) {
            case "on" -> CardShape.enabled = true;
            case "off" -> CardShape.enabled = false;
            case "check" -> CardShape.check = args.length > 2 && args[2].equals("on");
            case "reset" -> CardShape.resetStats();
            case "radius" -> CardShape.radius = parseIntBounded(sender, args.length > 2 ? args[2] : "", 0, 16);
            case "status" -> {}
            default -> throw new WrongUsageException(getCommandUsage(sender));
        }
        send(sender, CardShape.status());
    }

    private void verify(ICommandSender sender, String[] args) {
        if (args.length != 2) throw new WrongUsageException(getCommandUsage(sender));
        try {
            send(sender, CaptureVerify.run(overworld(), captureDir(args[1])));
        } catch (IOException e) {
            throw new CommandException("phiwg verify: " + e.getMessage());
        }
    }

    /**
     * Chunks around the origin on both sides of zero; chunks past each generator's first % 16777216 wrap; and the
     * first chunk found for each biome id within radius chunks of the origin (centres sampled every 4 chunks).
     */
    private static List<int[]> chunkSet(WorldServer world, int radius, StringBuilder found) {
        Map<Long, int[]> set = new LinkedHashMap<>();
        for (int x = -3; x <= 2; x++) for (int z = -3; z <= 2; z++) add(set, x, z);
        // octave 0 of a scale s wraps from |chunk| >= 2^24 / (4 s): 6128 (684.412), 20972 (200), 490266 (8.55515)
        int[][] far = { { 6200, 0 }, { -6200, 0 }, { 0, -6200 }, { 21000, 21000 }, { -21000, -21000 },
            { 491000, -491000 }, { -491000, 491000 }, { 1800000, -1800000 } };
        for (int[] c : far) add(set, c[0], c[1]);
        Map<Integer, int[]> byBiome = new LinkedHashMap<>();
        for (int x = -radius; x <= radius; x += 4) {
            for (int z = -radius; z <= radius; z += 4) {
                BiomeGenBase b = world.getWorldChunkManager()
                    .getBiomeGenAt(x * 16 + 8, z * 16 + 8);
                if (b != null && !byBiome.containsKey(b.biomeID)) byBiome.put(b.biomeID, new int[] { x, z });
            }
        }
        for (Map.Entry<Integer, int[]> e : byBiome.entrySet()) {
            add(set, e.getValue()[0], e.getValue()[1]);
            found.append(e.getKey())
                .append(' ');
        }
        return new ArrayList<>(set.values());
    }

    private static void add(Map<Long, int[]> set, int x, int z) {
        set.put(((long) x << 32) ^ (z & 0xFFFFFFFFL), new int[] { x, z });
    }

    private static Path captureDir(String name) {
        if (!name.matches("[A-Za-z0-9_.-]+")) throw new CommandException("phiwg: capture names are [A-Za-z0-9_.-]");
        return Paths.get(System.getProperty("user.home"), ".cache", "phiworldgen", "capture", name);
    }

    private static WorldServer overworld() {
        return MinecraftServer.getServer()
            .worldServerForDimension(0);
    }

    private static void send(ICommandSender sender, String text) {
        PhiWorldgen.LOG.info(text);
        for (String line : text.split("\n")) sender.addChatMessage(new ChatComponentText(line));
    }
}
