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
        int autoRuns = cfg.getInt(
            "runs",
            "autotest",
            0,
            0,
            1000,
            "Client only, for testing: make this many singleplayer worlds from the main menu, one after another, pregen and dump a square in each, append a line to ~/.cache/phiworldgen/autotest-summary.log, then quit (0: off)");
        int autoRadius = cfg
            .getInt("radius", "autotest", 16, 0, 64, "The pregen square's radius in chunks, 64 chunks east of spawn");
        String autoType = cfg
            .getString("worldType", "autotest", "RWG", "The world type of the autotest's worlds (RWG, default, ...)");
        String autoModes = cfg.getString(
            "cardModes",
            "autotest",
            "alternate",
            "alternate: pairs of runs on one seed, card off and on, the order alternating pair by pair; off or on: every run so, a seed each; a comma list (on, off, check) gives the runs their modes in turn, a seed each");
        String autoSeeds = cfg.getString(
            "seeds",
            "autotest",
            "",
            "Comma-separated seeds, one per pair (or per run); random where none is given");
        boolean autoQuit = cfg.getBoolean("quit", "autotest", true, "Quit the game after the last run");
        boolean autoCapture = cfg.getBoolean(
            "capture",
            "autotest",
            false,
            "Before each pregen, capture RWG's height stage (/phiwg rwgcap) to ~/.cache/phiworldgen/capture/autotest-RUN-MODE-SEED");
        boolean autoDump = cfg.getBoolean(
            "dump",
            "autotest",
            false,
            "Dump each pregen square (/phiwg pregen R dump): about 100 MB a world under GT New Horizons");
        if (cfg.hasChanged()) cfg.save();
        if (autoRuns > 0 && event.getSide()
            .isClient()) {
            io.github.lasimeri.phiworldgen.autotest.AutoTest t = new io.github.lasimeri.phiworldgen.autotest.AutoTest(
                autoRuns,
                autoRadius,
                autoType,
                autoModes,
                autoSeeds,
                autoQuit,
                autoCapture,
                autoDump);
            net.minecraftforge.common.MinecraftForge.EVENT_BUS.register(t);
            cpw.mods.fml.common.FMLCommonHandler.instance()
                .bus()
                .register(t);
            LOG.info(
                "phiwg autotest: {} runs of {} worlds, radius {}, card {}",
                autoRuns,
                autoType,
                autoRadius,
                autoModes);
        }
    }

    @Mod.EventHandler
    public void serverStarting(FMLServerStartingEvent event) {
        event.registerServerCommand(new CommandPhiwg());
    }

    @Mod.EventHandler
    public void serverStarted(FMLServerStartedEvent event) {
        LOG.info(GenProfile.report("spawn area, at server start"));
        LOG.info(
            io.github.lasimeri.phiworldgen.profile.Census.report(
                MinecraftServer.getServer()
                    .worldServerForDimension(0)));
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
                    Object gen = w.theChunkProviderServer.currentChunkProvider;
                    byte[] blob;
                    if (gen instanceof ChunkProviderGenerate p) blob = Capture.worldBlob(w, p);
                    else if (io.github.lasimeri.phiworldgen.capture.RwgCapture.refusal(w) == null)
                        blob = io.github.lasimeri.phiworldgen.capture.RwgCapture.worldBlob(w); // RWG: the height stage
                    else {
                        LOG.info(
                            "phiwg card: the overworld generator is {} ({}); the card is not used",
                            gen.getClass()
                                .getName(),
                            io.github.lasimeri.phiworldgen.capture.RwgCapture.refusal(w));
                        return;
                    }
                    CardClient c = CardClient.connect(
                        transport,
                        java.nio.file.Paths.get(shmFile),
                        (long) shmOffsetMiB << 20,
                        cardHost,
                        cardPort,
                        blob,
                        gen);
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
