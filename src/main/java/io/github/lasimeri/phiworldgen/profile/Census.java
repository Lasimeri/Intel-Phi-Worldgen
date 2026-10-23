package io.github.lasimeri.phiworldgen.profile;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

import net.minecraft.world.WorldServer;
import net.minecraft.world.gen.MapGenBase;
import net.minecraft.world.gen.NoiseGenerator;
import net.minecraftforge.common.MinecraftForge;
import net.minecraftforge.event.terraingen.ChunkProviderEvent;
import net.minecraftforge.event.terraingen.DecorateBiomeEvent;
import net.minecraftforge.event.terraingen.InitMapGenEvent;
import net.minecraftforge.event.terraingen.InitNoiseGensEvent;
import net.minecraftforge.event.terraingen.OreGenEvent;
import net.minecraftforge.event.terraingen.PopulateChunkEvent;

import cpw.mods.fml.common.eventhandler.Event;
import cpw.mods.fml.common.eventhandler.EventBus;
import cpw.mods.fml.common.eventhandler.EventPriority;
import cpw.mods.fml.common.eventhandler.IEventListener;
import cpw.mods.fml.common.registry.GameRegistry;
import cpw.mods.fml.relauncher.ReflectionHelper;

/** What else takes part in overworld generation in this game: hook listeners, world generators. See Census.md. */
public final class Census {

    private Census() {}

    public static String report(WorldServer world) {
        StringBuilder sb = new StringBuilder("== phiwg census: overworld generation in this game\n");
        Object gen = world.theChunkProviderServer.currentChunkProvider;
        sb.append("generator ")
            .append(
                gen.getClass()
                    .getName())
            .append(", world type ")
            .append(
                world.getWorldInfo()
                    .getTerrainType()
                    .getWorldTypeName())
            .append(", seed ")
            .append(world.getSeed())
            .append('\n');
        if (gen.getClass()
            .getName()
            .equals("rwg.world.ChunkGeneratorRealistic")) {
            MapGenBase caves = ((io.github.lasimeri.phiworldgen.latemixins.AccessorChunkGeneratorRealistic) gen)
                .phiwg$caves();
            sb.append("RWG caves ")
                .append(
                    caves.getClass()
                        .getName())
                .append(", their Random ")
                .append(randomClass(caves))
                .append('\n');
            sb.append(rwgBiomes(world));
        }
        EventBus forge = MinecraftForge.EVENT_BUS, terrain = MinecraftForge.TERRAIN_GEN_BUS,
            ore = MinecraftForge.ORE_GEN_BUS;
        listeners(sb, forge, new ChunkProviderEvent.ReplaceBiomeBlocks(null, 0, 0, null, null, null, null));
        listeners(sb, forge, new ChunkProviderEvent.InitNoiseField(null, null, 0, 0, 0, 0, 0, 0));
        listeners(sb, forge, new PopulateChunkEvent.Pre(null, null, null, 0, 0, false));
        listeners(sb, forge, new PopulateChunkEvent.Post(null, null, null, 0, 0, false));
        try { // its constructor is package-private
            java.lang.reflect.Constructor<InitMapGenEvent> c = InitMapGenEvent.class
                .getDeclaredConstructor(InitMapGenEvent.EventType.class, MapGenBase.class);
            c.setAccessible(true);
            listeners(sb, terrain, c.newInstance(null, null));
        } catch (ReflectiveOperationException e) {
            sb.append("InitMapGenEvent unreadable: ")
                .append(e)
                .append('\n');
        }
        listeners(sb, terrain, new InitNoiseGensEvent(null, null, new NoiseGenerator[0]));
        listeners(sb, terrain, new PopulateChunkEvent.Populate(null, null, null, 0, 0, false, null));
        listeners(sb, terrain, new DecorateBiomeEvent.Pre(null, null, 0, 0));
        listeners(sb, terrain, new DecorateBiomeEvent.Decorate(null, null, 0, 0, null));
        listeners(sb, terrain, new DecorateBiomeEvent.Post(null, null, 0, 0));
        listeners(sb, ore, new OreGenEvent.Pre(null, null, 0, 0));
        listeners(sb, ore, new OreGenEvent.GenerateMinable(null, null, null, 0, 0, null));
        listeners(sb, ore, new OreGenEvent.Post(null, null, 0, 0));
        try {
            java.util.Set<?> gens = ReflectionHelper.getPrivateValue(GameRegistry.class, null, "worldGenerators");
            sb.append("IWorldGenerator (populate, CPU) ")
                .append(gens.size())
                .append(':');
            for (Object g : gens) sb.append(' ')
                .append(
                    g.getClass()
                        .getName());
            sb.append('\n');
        } catch (RuntimeException e) {
            sb.append("IWorldGenerator list unreadable: ")
                .append(e)
                .append('\n');
        }
        return sb.toString();
    }

    private static void listeners(StringBuilder sb, EventBus bus, Event e) {
        List<String> found = new ArrayList<>();
        try {
            java.lang.reflect.Field f = EventBus.class.getDeclaredField("busID");
            f.setAccessible(true);
            for (IEventListener l : e.getListenerList()
                .getListeners(f.getInt(bus))) if (!(l instanceof EventPriority)) found.add(l.toString());
        } catch (ReflectiveOperationException | RuntimeException ex) {
            found.add("unreadable: " + ex);
        }
        String name = e.getClass()
            .getName();
        sb.append(name.substring(name.lastIndexOf('.') + 1))
            .append(' ')
            .append(found.size())
            .append(found.isEmpty() ? "" : ": " + String.join("; ", found))
            .append('\n');
    }

    /**
     * RWG's biome lists as ChunkManagerRealistic holds them in this game (RWG's own biomes plus the ones its
     * Support classes add for other mods), and every realistic biome: id, class, base, river and beach biome.
     */
    private static String rwgBiomes(WorldServer world) {
        StringBuilder sb = new StringBuilder();
        Object cmr = world.getWorldChunkManager();
        try {
            for (String list : new String[] { "biomes_snow", "biomes_cold", "biomes_hot", "biomes_wet", "biomes_small",
                "biomes_test" }) {
                java.lang.reflect.Field f = cmr.getClass()
                    .getDeclaredField(list);
                f.setAccessible(true);
                sb.append("RWG ")
                    .append(list)
                    .append(":");
                for (Object b : (java.util.List<?>) f.get(cmr)) sb.append(' ')
                    .append(rwgField(b, "biomeID"));
                sb.append('\n');
            }
            Class<?> base = Class.forName("rwg.biomes.realistic.RealisticBiomeBase");
            java.lang.reflect.Field all = base.getDeclaredField("biomeList");
            all.setAccessible(true);
            Object[] list = (Object[]) all.get(null);
            for (Object b : list) {
                if (b == null) continue;
                Object beach = rwgField(b, "beachBiome");
                sb.append(
                    String.format(
                        "RWG biome %s %s base %s river %s beach %s sub %s%n",
                        rwgField(b, "biomeID"),
                        b.getClass()
                            .getName(),
                        biomeId(rwgField(b, "baseBiome")),
                        biomeId(rwgField(b, "riverBiome")),
                        beach == null ? "-" : rwgField(beach, "biomeID"),
                        rwgField(b, "subID")));
            }
        } catch (ReflectiveOperationException | RuntimeException e) {
            sb.append("RWG biomes unreadable: ")
                .append(e)
                .append('\n');
        }
        return sb.toString();
    }

    private static Object rwgField(Object o, String name) throws ReflectiveOperationException {
        for (Class<?> c = o.getClass(); c != null; c = c.getSuperclass()) {
            try {
                java.lang.reflect.Field f = c.getDeclaredField(name);
                f.setAccessible(true);
                return f.get(o);
            } catch (NoSuchFieldException e) {
                // a superclass declares it
            }
        }
        throw new NoSuchFieldException(name);
    }

    private static String biomeId(Object b) {
        return b instanceof net.minecraft.world.biome.BiomeGenBase g ? g.biomeID + "(" + g.biomeName + ")"
            : String.valueOf(b);
    }

    private static String randomClass(MapGenBase g) {
        try {
            Random r = ReflectionHelper.getPrivateValue(MapGenBase.class, g, "field_75038_b", "rand");
            return r.getClass()
                .getName();
        } catch (RuntimeException e) {
            return "unreadable: " + e;
        }
    }
}
