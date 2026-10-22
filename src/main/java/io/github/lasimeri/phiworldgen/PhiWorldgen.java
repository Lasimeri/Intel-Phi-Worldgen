package io.github.lasimeri.phiworldgen;

import net.minecraft.server.MinecraftServer;
import net.minecraft.world.WorldServer;
import net.minecraft.world.gen.ChunkProviderGenerate;
import net.minecraftforge.common.config.Configuration;

import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import cpw.mods.fml.common.Mod;
import cpw.mods.fml.common.event.FMLPreInitializationEvent;
import cpw.mods.fml.common.event.FMLServerStartedEvent;
import cpw.mods.fml.common.event.FMLServerStartingEvent;
import cpw.mods.fml.common.event.FMLServerStoppingEvent;
import io.github.lasimeri.phiworldgen.capture.Capture;
import io.github.lasimeri.phiworldgen.card.CardClient;
import io.github.lasimeri.phiworldgen.card.CardShape;
import io.github.lasimeri.phiworldgen.command.CommandPhiwg;
import io.github.lasimeri.phiworldgen.profile.GenProfile;

/** The mod's entry point; worldgen runs on the server only, so any client may join. See PhiWorldgen.md. */
@Mod(
    modid = PhiWorldgen.MODID,
    name = "Intel Phi Worldgen",
    version = Tags.VERSION,
    acceptedMinecraftVersions = "[1.7.10]",
    acceptableRemoteVersions = "*")
public class PhiWorldgen {

    public static final String MODID = "phiworldgen";
    public static final Logger LOG = LogManager.getLogger(MODID);

    private static String cardHost = "127.0.0.1";
    private static int cardPort = 25600, shmOffsetMiB = 1024;
    private static String transport = "auto", shmFile = "/dev/shm/phi-hostmem";
    private static volatile boolean serving;

    @Mod.EventHandler
    public void preInit(FMLPreInitializationEvent event) {
        Configuration cfg = new Configuration(event.getSuggestedConfigurationFile());
        cfg.load();
        CardShape.enabled = cfg.getBoolean(
            "enabled",
            "card",
            true,
            "Compute the overworld's terrain shape on the Xeon Phi card (card/pwgd); the CPU does it when the card is unreachable or late");
        cardHost = cfg.getString("host", "card", "127.0.0.1", "Where pwgd is reached (the ssh -L forward)");
        cardPort = cfg.getInt("port", "card", 25600, 1, 65535, "pwgd's port");
        transport = cfg.getString(
            "transport",
            "card",
            "auto",
            "shm: the shared host-memory region phictl pins for the card through vfio, written by the card's DMA engine (docs/shm-protocol.md); tcp: the ssh forward; auto: shm, else tcp",
            new String[] { "auto", "shm", "tcp" });
        shmFile = cfg
            .getString("shmFile", "card", "/dev/shm/phi-hostmem", "The host side of the shared host-memory region");
        shmOffsetMiB = cfg.getInt(
            "shmOffsetMiB",
            "card",
            1024,
            0,
            65536,
            "Where this mod's part of the region starts: above the AVX-512 worker's 768 MiB, below the card's swap at 2 GiB");
        CardShape.radius = cfg.getInt(
            "prefetchRadius",
            "card",
            4,
            0,
            16,
            "On every chunk made, ask the card for the missing chunks within this many chunks of it, nearest first");
        CardShape.perChunk = cfg.getInt(
            "prefetchPerChunk",
            "card",
            16,
            1,
            256,
            "At most this many prefetch requests per chunk made, so one chunk never pays for a whole ring of biome lookups");
        CardShape.waitMs = cfg.getInt(
            "waitMs",
            "card",
            1,
            0,
            60000,
            "At most how long the server thread waits for an answer already asked for, before the CPU makes the chunk (a chunk never asked for is made by the CPU at once)");
        CardShape.check = cfg.getBoolean(
            "check",
            "card",
            false,
            "Compute every chunk on both and compare (the card's answer is not used): for testing");
        if (cfg.hasChanged()) cfg.save();
    }

    @Mod.EventHandler
    public void serverStarting(FMLServerStartingEvent event) {
        event.registerServerCommand(new CommandPhiwg());
    }

    @Mod.EventHandler
    public void serverStarted(FMLServerStartedEvent event) {
        LOG.info(GenProfile.report("spawn area, at server start"));
        GenProfile.reset();
        serving = true;
        Thread t = new Thread(PhiWorldgen::keepCardConnected, "phiwg-card-connect");
        t.setDaemon(true);
        t.start();
    }

    @Mod.EventHandler
    public void serverStopping(FMLServerStoppingEvent event) {
        serving = false;
        CardClient.close();
    }

    /** While the server runs: connect to pwgd whenever there is no connection, every 10 s. */
    private static void keepCardConnected() {
        boolean reported = false;
        while (serving) {
            if (CardShape.enabled && CardClient.get() == null) {
                try {
                    WorldServer w = MinecraftServer.getServer()
                        .worldServerForDimension(0);
                    if (!(w.theChunkProviderServer.currentChunkProvider instanceof ChunkProviderGenerate p)) {
                        LOG.info(
                            "phiwg card: the overworld generator is not ChunkProviderGenerate; the card is not used");
                        return;
                    }
                    CardClient c = CardClient.connect(
                        transport,
                        java.nio.file.Paths.get(shmFile),
                        (long) shmOffsetMiB << 20,
                        cardHost,
                        cardPort,
                        Capture.worldBlob(w, p),
                        p);
                    LOG.info(
                        "phiwg card: connected to pwgd through the {} link, {} card workers",
                        c.linkName(),
                        c.workers);
                    reported = false;
                } catch (IllegalStateException e) {
                    LOG.info("phiwg card: {}; the card is not used", e.getMessage());
                    return;
                } catch (Exception e) {
                    if (!reported) LOG.info(
                        "phiwg card: pwgd not reachable at {}:{} ({}); the CPU generates, retrying every 10 s",
                        cardHost,
                        cardPort,
                        e.toString());
                    reported = true;
                }
            }
            try {
                Thread.sleep(10000);
            } catch (InterruptedException e) {
                return;
            }
        }
    }
}
