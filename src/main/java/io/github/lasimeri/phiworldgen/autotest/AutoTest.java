package io.github.lasimeri.phiworldgen.autotest;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.security.SecureRandom;
import java.time.LocalDateTime;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.GuiMainMenu;
import net.minecraft.client.multiplayer.WorldClient;
import net.minecraft.server.MinecraftServer;
import net.minecraft.world.WorldServer;
import net.minecraft.world.WorldSettings;
import net.minecraft.world.WorldType;

import cpw.mods.fml.common.eventhandler.SubscribeEvent;
import cpw.mods.fml.common.gameevent.PlayerEvent;
import cpw.mods.fml.common.gameevent.TickEvent;
import io.github.lasimeri.phiworldgen.PhiWorldgen;
import io.github.lasimeri.phiworldgen.card.CardShape;
import io.github.lasimeri.phiworldgen.command.CommandPhiwg;
import io.github.lasimeri.phiworldgen.profile.Census;
import io.github.lasimeri.phiworldgen.profile.GenProfile;

/**
 * Singleplayer runs with nobody at the keyboard: from the main menu, a new world per run, a pregen square with its
 * dump, one summary line, back to the menu; the game quits after the last run. Client only. See AutoTest.md.
 */
public final class AutoTest {

    private final int runs, radius;
    private final String worldType, cardModes;
    private final long[] seeds;
    private final boolean quit, capture, dump;
    private final Path summary = Paths
        .get(System.getProperty("user.home"), ".cache", "phiworldgen", "autotest-summary.log");

    private volatile int run; // the next run to start
    private int titleTicks; // client ticks the title screen has been up, idle
    private volatile String launchedFolder; // the world the autotest made: it never touches any other
    private volatile boolean loggedIn, ran, done;
    private long cardDeadline; // a card run waits this long (System.nanoTime) for pwgd before its pregen
    private volatile String mode;
    private volatile long seed;

    public AutoTest(int runs, int radius, String worldType, String cardModes, String seedList, boolean quit,
        boolean capture, boolean dump) {
        this.runs = runs;
        this.radius = radius;
        this.worldType = worldType;
        this.cardModes = cardModes;
        this.quit = quit;
        this.capture = capture;
        this.dump = dump;
        // one seed per pair of runs (card off and on) unless the mode is fixed: a seed per run
        int n = cardModes.equals("alternate") ? (runs + 1) / 2 : runs;
        seeds = new long[n];
        String[] given = seedList.isEmpty() ? new String[0] : seedList.split(",");
        SecureRandom r = new SecureRandom();
        for (int i = 0; i < n; i++) seeds[i] = i < given.length ? Long.parseLong(given[i].trim()) : r.nextLong();
    }

    @SubscribeEvent
    public void onClientTick(TickEvent.ClientTickEvent e) {
        if (e.phase != TickEvent.Phase.END) return;
        Minecraft mc = Minecraft.getMinecraft();
        if (done) {
            done = false;
            // what the pause menu's "Save and Quit to Title" does
            mc.theWorld.sendQuittingDisconnectingPacket();
            mc.loadWorld((WorldClient) null);
            mc.displayGuiScreen(new GuiMainMenu());
            return;
        }
        // the title screen: vanilla's, or the one Custom Main Menu (GT New Horizons) puts in its place
        boolean title = mc.theWorld == null && mc.currentScreen != null
            && (mc.currentScreen instanceof GuiMainMenu || mc.currentScreen.getClass()
                .getName()
                .startsWith("lumien.custommainmenu."));
        if (!title) {
            titleTicks = 0;
            return;
        }
        if (++titleTicks < 40) return; // 2 s at the title screen between runs
        titleTicks = 0;
        if (run >= runs) {
            if (quit) {
                PhiWorldgen.LOG.info("phiwg autotest: {} runs done, quitting", runs);
                mc.shutdown();
                // the game can hang in its own shutdown (seen under GT New Horizons): end it 20 s later regardless
                Thread halt = new Thread(() -> {
                    try {
                        Thread.sleep(20000);
                    } catch (InterruptedException ex) {
                        return;
                    }
                    PhiWorldgen.LOG.info("phiwg autotest: the game did not exit in 20 s; halting");
                    Runtime.getRuntime()
                        .halt(0);
                }, "phiwg-autotest-halt");
                halt.setDaemon(true);
                halt.start();
            }
            return;
        }
        boolean alternate = cardModes.equals("alternate");
        int pair = alternate ? run / 2 : run;
        // pairs alternate their order: off, on, then on, off
        // a comma list gives each run its mode in turn: on, off, or check (the card on, both computed and compared)
        String[] list = cardModes.split(",");
        String m = list.length > 1 ? list[run % list.length].trim() : null;
        boolean on = m != null ? !m.equals("off")
            : alternate ? ((run % 2 == 0) == (pair % 2 == 1)) : cardModes.equals("on");
        if (m != null) CardShape.check = m.equals("check");
        mode = m != null ? m : on ? "on" : "off";
        seed = seeds[pair];
        CardShape.enabled = on;
        WorldType type = WorldType.parseWorldType(worldType);
        if (type == null) {
            PhiWorldgen.LOG.error("phiwg autotest: no world type {}; stopping", worldType);
            run = runs;
            return;
        }
        String folder = String.format(
            "phiwg-autotest-%s-%d-%s",
            LocalDateTime.now()
                .toString()
                .replace(':', '-'),
            run,
            mode);
        WorldSettings settings = new WorldSettings(seed, WorldSettings.GameType.CREATIVE, true, false, type);
        settings.enableCommands();
        loggedIn = false;
        ran = false;
        PhiWorldgen.LOG.info(
            "phiwg autotest: run {} of {}: world {}, type {}, seed {}, card {}",
            run + 1,
            runs,
            folder,
            worldType,
            seed,
            mode);
        run++;
        launchedFolder = folder;
        mc.launchIntegratedServer(folder, folder, settings);
    }

    @SubscribeEvent
    public void onLogin(PlayerEvent.PlayerLoggedInEvent e) {
        loggedIn = true;
    }

    /** On the integrated server's thread, the tick after the player is in: census, pregen and dump, summary. */
    @SubscribeEvent
    public void onServerTick(TickEvent.ServerTickEvent e) {
        if (e.phase != TickEvent.Phase.END || !loggedIn || ran) return;
        MinecraftServer server = MinecraftServer.getServer();
        if (server == null || !server.getFolderName()
            .equals(launchedFolder)) return; // a world someone opened by hand
        // a card run starts once the card is connected (keepCardConnected), or after 30 s without it
        if (!mode.equals("off") && io.github.lasimeri.phiworldgen.card.CardClient.get() == null) {
            if (cardDeadline == 0) cardDeadline = System.nanoTime() + 30_000_000_000L;
            if (System.nanoTime() < cardDeadline) return;
            PhiWorldgen.LOG.warn("phiwg autotest: no card connection after 30 s; the run goes on without it");
        }
        cardDeadline = 0;
        ran = true;
        WorldServer world = server.worldServerForDimension(0);
        PhiWorldgen.LOG.info(Census.report(world));
        String name = String.format("autotest-%d-%s-%d", run - 1, mode, seed);
        // RWG's height stage on a chunk set, before the pregen touches the world
        if (capture) server.getCommandManager()
            .executeCommand(server, "phiwg rwgcap " + name + " 256");
        server.getCommandManager()
            .executeCommand(server, "phiwg card reset");
        CommandPhiwg.lastSeconds = 0; // a pregen that throws leaves them at zero
        CommandPhiwg.lastChunks = 0;
        server.getCommandManager()
            .executeCommand(server, String.format("phiwg pregen %d%s", radius, dump ? " dump " + name : ""));
        double s = CommandPhiwg.lastSeconds;
        long n = CommandPhiwg.lastChunks;
        String line = String.format(
            "%s run %d mode %s seed %d type %s generator %s radius %d chunks %d s %.3f chunks/s %.1f provide_ms %.3f populate_ms %.3f populate_calls %d dump %s%n",
            LocalDateTime.now(),
            run - 1,
            mode,
            seed,
            worldType,
            world.theChunkProviderServer.currentChunkProvider.getClass()
                .getSimpleName(),
            radius,
            n,
            s,
            s == 0 ? 0.0 : n / s,
            GenProfile.msPerCall(GenProfile.PROVIDE),
            GenProfile.msPerCall(GenProfile.POPULATE),
            GenProfile.calls(GenProfile.POPULATE),
            dump ? name : "-");
        try {
            Files.createDirectories(summary.getParent());
            Files.write(
                summary,
                line.getBytes(StandardCharsets.UTF_8),
                StandardOpenOption.CREATE,
                StandardOpenOption.APPEND);
        } catch (IOException ex) {
            PhiWorldgen.LOG.error("phiwg autotest: summary not written: {}", ex.toString());
        }
        PhiWorldgen.LOG.info("phiwg autotest: {}", line.trim());
        server.getCommandManager()
            .executeCommand(server, "phiwg card status");
        done = true;
    }
}
